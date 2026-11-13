#include "RHI/OpenGL/OpenGLBindGroup.h"

#include "RHI/Framebuffer.h"
#include "RHI/Texture.h"
#include "RHI/OpenGL/OpenGLUniformBuffer.h"

namespace Opaax
{

    OpenGLBindGroup::OpenGLBindGroup(const BindGroupLayout& InLayout)
        : m_Layout(InLayout)
        , m_UniformBuffers(InLayout.UniformBufferBindings.size(), nullptr)
        , m_Textures(InLayout.TextureSlotCount)
    {
        if (!m_Layout.IsValid())
        {
            OPAAX_LOG(LogOpenGLBindGroup, Error,
                      "A bind group's layout lists a uniform binding point twice: only one of the two blocks is bound. "
                      "Give each block its own point.");
        }
    }

    void OpenGLBindGroup::SetUniformBuffer(IUniformBuffer& InUniformBuffer)
    {
        // The device makes every uniform buffer of this backend.
        OpenGLUniformBuffer& lBuffer = static_cast<OpenGLUniformBuffer&>(InUniformBuffer);
        const Uint32         lIndex  = m_Layout.FindUniformBuffer(lBuffer.GetBinding());

        if (lIndex == BindGroupLayout::NOT_FOUND)
        {
            OPAAX_LOG(LogOpenGLBindGroup, Error,
                      "A uniform buffer made for binding point {} was given to a bind group whose layout does not "
                      "list it: add {} to the layout's UniformBufferBindings.", lBuffer.GetBinding(), lBuffer.GetBinding());
            return;
        }

        m_UniformBuffers[lIndex] = &lBuffer;
    }

    void OpenGLBindGroup::SetTexture(const Uint32 InSlot, ITexture2D& InTexture)
    {
        if (InSlot < m_Textures.size())
        {
            m_Textures[InSlot] = TextureSlot{ &InTexture, nullptr };
        }
    }

    void OpenGLBindGroup::SetTexture(const Uint32 InSlot, const IFramebuffer& InFramebuffer)
    {
        if (InSlot < m_Textures.size())
        {
            m_Textures[InSlot] = TextureSlot{ nullptr, &InFramebuffer };
        }
    }

    void OpenGLBindGroup::Bind() const
    {
        for (const OpenGLUniformBuffer* lBuffer : m_UniformBuffers)
        {
            if (lBuffer != nullptr) { lBuffer->Bind(); }
        }

        for (Uint32 i = 0; i < static_cast<Uint32>(m_Textures.size()); ++i)
        {
            const TextureSlot& lSlot = m_Textures[i];

            if (lSlot.Texture != nullptr)          { lSlot.Texture->Bind(i); }
            else if (lSlot.Framebuffer != nullptr) { lSlot.Framebuffer->BindColorTexture(i); }
        }
    }
}
