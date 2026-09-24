#pragma once

#include "RHI/BindGroup.h"

#if OPAAX_HAS_VULKAN

#include "Core/OpaaxTypes.h"

#include <vulkan/vulkan.h>

namespace Opaax
{
    class VulkanUniformBuffer;
    class VulkanTexture2D;

    // =============================================================================
    // VulkanBindGroup
    // =============================================================================
    /**
     * @class VulkanBindGroup
     *
     * IBindGroup for Vulkan (descriptor set 0 of the sprite pipeline). SetUniformBuffer/SetTexture
     * only cache pointers (same shape as OpenGLBindGroup); the actual descriptor write + bind
     * happens in BindInto, called by VulkanCommandBuffer per flush.
     *
     * Holds a RING of pre-allocated descriptor sets PER frame-in-flight: each flush updates +
     * binds the NEXT set so a mid-frame texture change (multi-flush / world-then-overlay) does not
     * clobber a draw already recorded but not yet executed. The UBO binding uses the uniform
     * buffer's current ring offset, so a pass's descriptor points at that pass's view-projection.
     */
    class VulkanBindGroup final : public IBindGroup
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        explicit VulkanBindGroup(const BindGroupLayout& InLayout);
        ~VulkanBindGroup() override;

        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin IBindGroup interface
    public:
        void SetUniformBuffer(IUniformBuffer& InUniformBuffer)    override;
        void SetTexture(Uint32 InSlot, ITexture2D& InTexture)     override;
        //~End IBindGroup interface

        // =============================================================================
        // Functions — consumed by VulkanCommandBuffer
        // =============================================================================
    public:
        // Update the next ring descriptor set with the cached UBO + textures, then bind it.
        void BindInto(VkCommandBuffer InCmd, VkPipelineLayout InPipelineLayout);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        VkDevice              m_Device     = VK_NULL_HANDLE;   // borrowed
        VkDescriptorPool      m_Pool       = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_SetLayout  = VK_NULL_HANDLE;
        Uint32                m_TextureCount = 0;

        TDynArray<VkDescriptorSet> m_Sets;   // [frameSlot * RING + ringCursor]

        VulkanUniformBuffer*           m_UBO = nullptr;
        TDynArray<VulkanTexture2D*>    m_Textures;   // size = m_TextureCount

        Uint64 m_FrameGen   = ~0ull;
        Uint32 m_RingCursor = 0;
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
