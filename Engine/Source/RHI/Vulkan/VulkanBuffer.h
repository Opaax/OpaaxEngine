#pragma once

#include "RHI/Buffer.h"

#if OPAAX_HAS_VULKAN

#include "RHI/Vulkan/VulkanSwapchain.h"   // OPAAX_FRAMES_IN_FLIGHT

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

namespace Opaax
{
    // =============================================================================
    // VulkanVertexBuffer
    // =============================================================================
    /**
     * @class VulkanVertexBuffer
     *
     * Dynamic vertex buffer backed by one host-visible+mapped VMA allocation PER frame-in-flight
     * (so a write never races a frame still reading the previous contents). Within a frame, a
     * write OFFSET advances per SetData (per flush / per pass) — Renderer2D flushes the world then
     * the overlay into the same logical buffer, and Vulkan executes both draws at submit, so each
     * must occupy a distinct region. The offset resets when the frame generation changes.
     *
     * BindVertexArray (in VulkanCommandBuffer) reads GetBuffer(frameSlot) + GetLastBindOffset() to
     * record vkCmdBindVertexBuffers at the region the matching SetData just wrote.
     */
    class VulkanVertexBuffer final : public IVertexBuffer
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        explicit VulkanVertexBuffer(Uint32 InSize);                 // dynamic
        VulkanVertexBuffer(const float* InVertices, Uint32 InSize); // static (one buffer)
        ~VulkanVertexBuffer() override;

        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin IVertexBuffer interface
    public:
        void Bind()   const override {}
        void Unbind() const override {}

        void SetData(const void* InData, Uint32 InSize) override;
        void SetLayout(const BufferLayout& InLayout)    override { m_Layout = InLayout; }

        const BufferLayout& GetLayout() const override { return m_Layout; }
        //~End IVertexBuffer interface

        // =============================================================================
        // Get — consumed by VulkanCommandBuffer
        // =============================================================================
    public:
        VkBuffer     GetBuffer(Uint32 InFrameSlot) const noexcept { return m_Buffers[InFrameSlot]; }
        VkDeviceSize GetLastBindOffset()           const noexcept { return m_LastBindOffset; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        BufferLayout m_Layout;
        VmaAllocator m_Allocator = nullptr;
        Uint32       m_Capacity  = 0;
        bool         m_Static    = false;

        VkBuffer      m_Buffers[OPAAX_FRAMES_IN_FLIGHT] = {};
        VmaAllocation m_Allocs [OPAAX_FRAMES_IN_FLIGHT] = {};
        void*         m_Mapped [OPAAX_FRAMES_IN_FLIGHT] = {};

        Uint64       m_FrameGen       = ~0ull;   // force a reset on the first SetData of a frame
        VkDeviceSize m_WriteOffset    = 0;       // advancing cursor within the current frame
        VkDeviceSize m_LastBindOffset = 0;       // offset the most recent SetData wrote at
    };

    // =============================================================================
    // VulkanIndexBuffer
    // =============================================================================
    /**
     * @class VulkanIndexBuffer
     *
     * Static index buffer — quad indices never change, so one host-visible VMA allocation uploaded
     * once at construction is shared across all frames (read-only). Small (a few thousand Uint32).
     */
    class VulkanIndexBuffer final : public IIndexBuffer
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        VulkanIndexBuffer(const Uint32* InIndices, Uint32 InCount);
        ~VulkanIndexBuffer() override;

        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin IIndexBuffer interface
    public:
        void Bind()   const override {}
        void Unbind() const override {}

        Uint32 GetCount() const override { return m_Count; }
        //~End IIndexBuffer interface

        // =============================================================================
        // Get — consumed by VulkanCommandBuffer
        // =============================================================================
    public:
        VkBuffer GetBuffer() const noexcept { return m_Buffer; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        VmaAllocator  m_Allocator = nullptr;
        VkBuffer      m_Buffer    = VK_NULL_HANDLE;
        VmaAllocation m_Alloc     = nullptr;
        Uint32        m_Count     = 0;
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
