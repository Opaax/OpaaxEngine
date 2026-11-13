#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class IUniformBuffer;
    class ITexture2D;
    class IFramebuffer;

    // =============================================================================
    // BindGroupLayout
    // =============================================================================
    /**
     * The resource slots of a bind group: the uniform blocks a pass's shader reads, by binding
     * point, and its sampler array.
     */
    struct BindGroupLayout
    {
        /** The binding point of each uniform block (layout(std140, binding = N)). No point twice. */
        TDynArray<Uint32> UniformBufferBindings;

        /** Length of the sampler array: texture units 0 to this, excluded. */
        Uint32 TextureSlotCount = 0;

        /** Returned by FindUniformBuffer for a binding point the layout does not have. */
        static constexpr Uint32 NOT_FOUND = 0xFFFFFFFFu;

        /** Where a binding point is in UniformBufferBindings, or NOT_FOUND. */
        Uint32 FindUniformBuffer(const Uint32 InBinding) const noexcept
        {
            for (Uint32 i = 0; i < static_cast<Uint32>(UniformBufferBindings.size()); ++i)
            {
                if (UniformBufferBindings[i] == InBinding)
                {
                    return i;
                }
            }

            return NOT_FOUND;
        }

        /** False when a binding point is listed twice: one of the two blocks would never be bound. */
        bool IsValid() const noexcept
        {
            for (Uint32 i = 0; i < static_cast<Uint32>(UniformBufferBindings.size()); ++i)
            {
                for (Uint32 j = i + 1; j < static_cast<Uint32>(UniformBufferBindings.size()); ++j)
                {
                    if (UniformBufferBindings[i] == UniformBufferBindings[j])
                    {
                        return false;
                    }
                }
            }

            return true;
        }
    };

    // =============================================================================
    // IBindGroup
    // =============================================================================
    /**
     * What a pass's shader reads, bound together (a descriptor set): its uniform buffers and its
     * textures. Fill it with SetUniformBuffer and SetTexture, then bind it before drawing
     * (ICommandBuffer::BindBindGroup): binding it is what puts each resource at its point, so a
     * pass reads what its own group holds, whatever was bound before. Textures may change
     * between draws. The group does not own what it holds.
     * Created by IRHIDevice::CreateBindGroup.
     */
    class IBindGroup
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IBindGroup() = default;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Puts a uniform buffer at its own binding point (the one it was created for).
         * Refused, with an Error line, when the layout does not list that point.
         */
        virtual void SetUniformBuffer(IUniformBuffer& InUniformBuffer)   = 0;

        /** Puts a texture in a sampler slot. Ignored past the layout's slot count. */
        virtual void SetTexture(Uint32 InSlot, ITexture2D& InTexture)    = 0;

        /**
         * Puts a framebuffer's colour in a sampler slot: what it holds when the group is bound,
         * so it follows the framebuffer's resizes. Ignored past the layout's slot count.
         */
        virtual void SetTexture(Uint32 InSlot, const IFramebuffer& InFramebuffer) = 0;
    };

} // namespace Opaax
