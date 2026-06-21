#include "VulkanUniformBuffer.h"

#if OPAAX_HAS_VULKAN

#include "VulkanFrameContext.h"
#include "VulkanDevice.h"
#include "Core/Config/EngineConfig.h"
#include "Core/Log/OpaaxLog.h"

#include <cstring>

namespace Opaax
{
    // NOTE: the IUniformBuffer::Create factory dispatch lives in RHI/BackendFactory.cpp.

    namespace
    {
        VkDeviceSize AlignUp(VkDeviceSize InValue, VkDeviceSize InAlign)
        {
            if (InAlign == 0) { return InValue; }
            return (InValue + InAlign - 1) & ~(InAlign - 1);
        }
    }

    VulkanUniformBuffer::VulkanUniformBuffer(Uint32 InSize, Uint32 /*InBinding*/)
        : m_Allocator(VulkanFrameContext::Allocator()), m_BlockSize(InSize)
    {
        // Round each ring block up to the device's UBO offset alignment so every descriptor offset
        // is legal.
        VulkanDevice* lDevice = VulkanFrameContext::Device();
        OPAAX_CORE_ASSERT(lDevice)

        VkPhysicalDeviceProperties lProps{};
        vkGetPhysicalDeviceProperties(lDevice->GetPhysicalDevice(), &lProps);
        const VkDeviceSize lAlign = lProps.limits.minUniformBufferOffsetAlignment;

        m_Aligned = AlignUp(m_BlockSize, lAlign);

        // Ring depth is config-tunable (render.vulkanFrameRing, default 64); matches VulkanBindGroup.
        m_RingDepth = EngineConfig::VulkanFrameRing();
        const VkDeviceSize lTotal = m_Aligned * m_RingDepth;

        for (Uint32 i = 0; i < OPAAX_FRAMES_IN_FLIGHT; ++i)
        {
            VkBufferCreateInfo lInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            lInfo.size  = lTotal;
            lInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

            VmaAllocationCreateInfo lAllocCI{};
            lAllocCI.usage = VMA_MEMORY_USAGE_AUTO;
            lAllocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                           | VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VmaAllocationInfo lOut{};
            if (vmaCreateBuffer(m_Allocator, &lInfo, &lAllocCI, &m_Buffers[i], &m_Allocs[i], &lOut) != VK_SUCCESS)
            {
                OPAAX_CORE_ERROR("VulkanUniformBuffer: vmaCreateBuffer failed.");
                continue;
            }
            m_Mapped[i] = lOut.pMappedData;
        }
    }

    VulkanUniformBuffer::~VulkanUniformBuffer()
    {
        for (Uint32 i = 0; i < OPAAX_FRAMES_IN_FLIGHT; ++i)
        {
            if (m_Buffers[i]) { vmaDestroyBuffer(m_Allocator, m_Buffers[i], m_Allocs[i]); }
        }
    }

    void VulkanUniformBuffer::SetData(const void* InData, Uint32 InSize, Uint32 InOffset)
    {
        const Uint64 lGen = VulkanFrameContext::Generation();
        if (lGen != m_FrameGen)
        {
            m_FrameGen   = lGen;
            m_RingCursor = 0;
        }

        // Ring exhausted: FAIL LOUD, never wrap (same hazard as VulkanBindGroup — wrapping rewrites a
        // UBO block a recorded draw still references). Skip the write: m_CurrentByteOffset keeps the
        // last valid slot, so the bound descriptor reads prior (valid) data rather than torn bytes.
        if (m_RingCursor >= m_RingDepth)
        {
            OPAAX_CORE_ERROR("VulkanUniformBuffer: frame exceeded {} UBO ring writes — skipping "
                             "(raise render.vulkanFrameRing).", m_RingDepth);
            OPAAX_CORE_ASSERT(false)
            return;
        }

        m_CurrentByteOffset = m_Aligned * m_RingCursor;

        const Uint32 lSlot = VulkanFrameContext::FrameSlot();
        if (m_Mapped[lSlot])
        {
            std::memcpy(static_cast<Uint8*>(m_Mapped[lSlot]) + m_CurrentByteOffset + InOffset, InData, InSize);
        }

        ++m_RingCursor;
    }

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
