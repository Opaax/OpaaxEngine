#pragma once

#include "RHI/BindGroup.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class IUniformBuffer;
    class ITexture2D;

    /**
     * @class OpenGLBindGroup
     *
     * OpenGL IBindGroup: stores the camera UBO + the batch's texture pointers and binds the
     * texture units when the command buffer binds it (Bind). The UBO is bound to its binding
     * point at construction by OpenGLUniformBuffer, so it needs no per-bind work here — the
     * pointer is kept for parity with a descriptor-set backend (Vulkan).
     */
    class OPAAX_API OpenGLBindGroup final : public IBindGroup
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
        // Backend-internal: called by OpenGLCommandBuffer::BindBindGroup. Binds each set
        // texture to its unit.
        void Bind() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        IUniformBuffer*        m_UniformBuffer = nullptr;   // bound at its own construction (GL)
        TDynArray<ITexture2D*> m_Textures;                  // sized to layout.TextureSlotCount
    };
}
