#pragma once

#include "RHI/UniformBuffer.h"

#if OPAAX_HAS_VULKAN

#include "RHI/Vulkan/VulkanSwapchain.h"   // OPAAX_FRAMES_IN_FLIGHT

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

namespace Opaax
{
    // =============================================================================
    // VulkanUniformBuffer
    // =============================================================================
    /**
     * @class VulkanUniformBuffer
     *
     * Uniform buffer backed by one host-visible+mapped VMA allocation PER frame-in-flight, each
     * holding a RING of aligned blocks. The camera UBO is rewritten on every Renderer2D::Begin
     * (world camera, then overlay screen-space camera) — two writes per frame minimum — so each
     * write lands in the NEXT ring slot. The bind group reads GetBuffer(frameSlot) +
     * GetCurrentByteOffset() to point that pass's descriptor at the matching block. The ring
     * cursor resets when the frame generation changes.
     */
    class VulkanUniformBuffer final : public IUniformBuffer
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        VulkanUniformBuffer(Uint32 InSize, Uint32 InBinding);
        ~VulkanUniformBuffer() override;

        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin IUniformBuffer interface
    public:
        void SetData(const void* InData, Uint32 InSize, Uint32 InOffset = 0) override;
        //~End IUniformBuffer interface

        // =============================================================================
        // Get — consumed by VulkanBindGroup
        // =============================================================================
    public:
        VkBuffer     GetBuffer(Uint32 InFrameSlot) const noexcept { return m_Buffers[InFrameSlot]; }
        VkDeviceSize GetCurrentByteOffset()        const noexcept { return m_CurrentByteOffset; }
        VkDeviceSize GetBlockSize()                const noexcept { return m_BlockSize; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        VmaAllocator m_Allocator = nullptr;
        VkDeviceSize m_BlockSize = 0;   // logical block size (sizeof the std140 struct)
        VkDeviceSize m_Aligned   = 0;   // block size rounded up to minUniformBufferOffsetAlignment

        VkBuffer      m_Buffers[OPAAX_FRAMES_IN_FLIGHT] = {};
        VmaAllocation m_Allocs [OPAAX_FRAMES_IN_FLIGHT] = {};
        void*         m_Mapped [OPAAX_FRAMES_IN_FLIGHT] = {};

        Uint64       m_FrameGen          = ~0ull;
        Uint32       m_RingCursor        = 0;
        VkDeviceSize m_CurrentByteOffset = 0;   // byte offset of the slot the last SetData wrote
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
