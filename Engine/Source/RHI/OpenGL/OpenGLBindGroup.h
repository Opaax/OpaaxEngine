#pragma once

#include "RHI/BindGroup.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class OpenGLUniformBuffer;

    OPAAX_LOG_CATEGORY(OpenGLBindGroup);

    /**
     * OpenGL IBindGroup: keeps the uniform buffers and the textures, and binds them all in Bind:
     * each buffer to its binding point, each texture to its unit.
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
        void SetTexture(Uint32 InSlot, const IFramebuffer& InFramebuffer) override;
        //~End IBindGroup interface

        // =============================================================================
        // Function
        // =============================================================================
    public:
        // Called by OpenGLCommandBuffer::BindBindGroup.
        void Bind() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** A sampler slot holds a texture or a framebuffer's colour: at most one is set. */
        struct TextureSlot
        {
            ITexture2D*         Texture     = nullptr;
            const IFramebuffer* Framebuffer = nullptr;
        };

        BindGroupLayout                 m_Layout;
        TDynArray<OpenGLUniformBuffer*> m_UniformBuffers;   // one per binding of the layout, same order
        TDynArray<TextureSlot>          m_Textures;         // sized to TextureSlotCount
    };
}
