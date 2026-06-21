#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class IUniformBuffer;
    class ITexture2D;

    // =============================================================================
    // BindGroupLayout
    // =============================================================================
    /**
     * @struct BindGroupLayout
     *
     * Describes the resource slots a bind group exposes — the input to IBindGroup::Create.
     * Minimal on purpose: the engine's single sprite pipeline needs one camera UBO plus a
     * fixed-size sampler array. Grow only when a second pipeline needs a different shape.
     */
    struct BindGroupLayout
    {
        Uint32 UniformBufferBinding = 0;   // binding point of the camera UBO (layout(binding = N))
        Uint32 TextureSlotCount     = 0;   // length of the sampler array (sampler2D[N])
    };

    // =============================================================================
    // IBindGroup
    // =============================================================================
    /**
     * @interface IBindGroup
     *
     * Backend-agnostic bundle of shader-visible resources (a descriptor set). Consumers fill
     * it (SetUniformBuffer / SetTexture) then bind it on the command buffer before drawing.
     * On OpenGL this binds the UBO base + texture units; on Vulkan it maps to a VkDescriptorSet.
     * The concrete impl is selected by IBindGroup::Create, defined in the active backend's TU.
     *
     * The texture slots are mutable between draws (Renderer2D rebinds its batch's textures each
     * flush) — backends that need immutable descriptors update per flush internally.
     */
    class OPAAX_API IBindGroup
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IBindGroup() = default;

        // =============================================================================
        // Factory
        // =============================================================================
    public:
        static UniquePtr<IBindGroup> Create(const BindGroupLayout& InLayout);

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        virtual void SetUniformBuffer(IUniformBuffer& InUniformBuffer)   = 0;
        virtual void SetTexture(Uint32 InSlot, ITexture2D& InTexture)    = 0;

        // Per-frame descriptor-ring usage so far (peak read by Renderer2D into RenderStats). 0 for
        // backends with no per-frame ring (OpenGL); Vulkan returns its current ring cursor. Lets the
        // neutral renderer surface ring pressure without a backend query.
        virtual Uint32 GetRingHighWater() const { return 0; }
    };

} // namespace Opaax
