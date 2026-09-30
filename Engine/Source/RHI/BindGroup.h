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
     * The resource slots of a bind group: one uniform buffer and a sampler array.
     */
    struct BindGroupLayout
    {
        Uint32 UniformBufferBinding = 0;   // binding point of the camera UBO
        Uint32 TextureSlotCount     = 0;   // length of the sampler array
    };

    // =============================================================================
    // IBindGroup
    // =============================================================================
    /**
     * Shader resources bound together (a descriptor set): fill it with SetUniformBuffer /
     * SetTexture, then bind it before drawing. Textures may change between draws.
     * Created by IRHIDevice::CreateBindGroup.
     */
    class OPAAX_API IBindGroup
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IBindGroup() = default;

        // =============================================================================
        // Functions
        // =============================================================================
        // Created via IRHIDevice::CreateBindGroup.
    public:
        virtual void SetUniformBuffer(IUniformBuffer& InUniformBuffer)   = 0;
        virtual void SetTexture(Uint32 InSlot, ITexture2D& InTexture)    = 0;
    };

} // namespace Opaax
