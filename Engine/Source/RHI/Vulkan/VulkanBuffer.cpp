#include "VulkanBuffer.h"

#if OPAAX_HAS_VULKAN

#include "VulkanFrameContext.h"
#include "Core/Log/OpaaxLog.h"

#include <cstring>

namespace Opaax
{
    // NOTE: the I*::Create factory dispatch lives in RHI/BackendFactory.cpp.

    namespace
    {
        // Create one host-visible + persistently-mapped buffer. Returns false on failure (logged).
        bool CreateMappedBuffer(VmaAllocator InAllocator, VkDeviceSize InSize, VkBufferUsageFlags InUsage,
                                VkBuffer& OutBuffer, VmaAllocation& OutAlloc, void*& OutMapped)
        {
            VkBufferCreateInfo lInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            lInfo.size  = InSize;
            lInfo.usage = InUsage;

            VmaAllocationCreateInfo lAllocCI{};
            lAllocCI.usage = VMA_MEMORY_USAGE_AUTO;
            lAllocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                           | VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VmaAllocationInfo lOutInfo{};
            if (vmaCreateBuffer(InAllocator, &lInfo, &lAllocCI, &OutBuffer, &OutAlloc, &lOutInfo) != VK_SUCCESS)
            {
                OPAAX_CORE_ERROR("VulkanBuffer: vmaCreateBuffer failed ({} bytes).", static_cast<Uint64>(InSize));
                return false;
            }
            OutMapped = lOutInfo.pMappedData;
            return true;
        }
    }

    // =============================================================================
    // VulkanVertexBuffer
    // =============================================================================

    VulkanVertexBuffer::VulkanVertexBuffer(Uint32 InSize)
        : m_Allocator(VulkanFrameContext::Allocator()), m_Capacity(InSize)
    {
        for (Uint32 i = 0; i < OPAAX_FRAMES_IN_FLIGHT; ++i)
        {
            CreateMappedBuffer(m_Allocator, InSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                               m_Buffers[i], m_Allocs[i], m_Mapped[i]);
        }
    }

    VulkanVertexBuffer::VulkanVertexBuffer(const float* InVertices, Uint32 InSize)
        : m_Allocator(VulkanFrameContext::Allocator()), m_Capacity(InSize), m_Static(true)
    {
        // Static path (not used by Renderer2D's dynamic batch VBO): one buffer, uploaded once.
        if (CreateMappedBuffer(m_Allocator, InSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                               m_Buffers[0], m_Allocs[0], m_Mapped[0]) && m_Mapped[0] && InVertices)
        {
            std::memcpy(m_Mapped[0], InVertices, InSize);
        }
    }

    VulkanVertexBuffer::~VulkanVertexBuffer()
    {
        const Uint32 lCount = m_Static ? 1u : OPAAX_FRAMES_IN_FLIGHT;
        for (Uint32 i = 0; i < lCount; ++i)
        {
            if (m_Buffers[i]) { vmaDestroyBuffer(m_Allocator, m_Buffers[i], m_Allocs[i]); }
        }
    }

    void VulkanVertexBuffer::SetData(const void* InData, Uint32 InSize)
    {
        if (m_Static) { return; }   // static buffers are write-once at construction

        // New frame → reset the write cursor (the slot we are about to write completed its prior
        // GPU use; its in-flight fence was waited in AcquireNextImage).
        const Uint64 lGen = VulkanFrameContext::Generation();
        if (lGen != m_FrameGen)
        {
            m_FrameGen    = lGen;
            m_WriteOffset = 0;
        }

        if (m_WriteOffset + InSize > m_Capacity)
        {
            OPAAX_CORE_ERROR("VulkanVertexBuffer: frame vertex data ({} + {}) exceeds capacity {} — "
                             "wrapping (frame will render incorrectly). Raise MAX_QUADS or the ring.",
                             static_cast<Uint64>(m_WriteOffset), InSize, m_Capacity);
            m_WriteOffset = 0;
        }

        const Uint32 lSlot = VulkanFrameContext::FrameSlot();
        if (m_Mapped[lSlot])
        {
            std::memcpy(static_cast<Uint8*>(m_Mapped[lSlot]) + m_WriteOffset, InData, InSize);
        }

        m_LastBindOffset = m_WriteOffset;
        m_WriteOffset   += InSize;
    }

    // =============================================================================
    // VulkanIndexBuffer
    // =============================================================================

    VulkanIndexBuffer::VulkanIndexBuffer(const Uint32* InIndices, Uint32 InCount)
        : m_Allocator(VulkanFrameContext::Allocator()), m_Count(InCount)
    {
        const VkDeviceSize lSize = static_cast<VkDeviceSize>(InCount) * sizeof(Uint32);

        void* lMapped = nullptr;
        if (CreateMappedBuffer(m_Allocator, lSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                               m_Buffer, m_Alloc, lMapped) && lMapped && InIndices)
        {
            std::memcpy(lMapped, InIndices, lSize);
        }
    }

    VulkanIndexBuffer::~VulkanIndexBuffer()
    {
        if (m_Buffer) { vmaDestroyBuffer(m_Allocator, m_Buffer, m_Alloc); }
    }

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
