#pragma once

#include "RHI/BindGroup.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class IUniformBuffer;
    class ITexture2D;

    /**
     * OpenGL IBindGroup: keeps the camera UBO and the textures, and binds the texture units in
     * Bind. The UBO is bound at its own construction.
     */
    class OpenGLBindGroup final : public IBindGroup
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        explicit OpenGLBindGroup(const BindGroupLayout& InLayout);
        ~OpenGLBindGroup() override = default;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IBindGroup interface
    public:
        void SetUniformBuffer(IUniformBuffer& InUniformBuffer) override;
        void SetTexture(Uint32 InSlot, ITexture2D& InTexture)  override;
        //~End IBindGroup interface

        // =============================================================================
        // Function
        // =============================================================================
    public:
        // Called by OpenGLCommandBuffer::BindBindGroup: binds each texture to its unit.
        void Bind() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        IUniformBuffer*        m_UniformBuffer = nullptr;   // bound at its own construction
        TDynArray<ITexture2D*> m_Textures;                  // sized to TextureSlotCount
    };
}
