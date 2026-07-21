#include "OpenGLBindGroup.h"

#include "RHI/Texture.h"
#include "RHI/UniformBuffer.h"

namespace Opaax
{
    // NOTE: the IBindGroup::Create factory dispatch lives in RHI/BackendFactory.cpp.

    OpenGLBindGroup::OpenGLBindGroup(const BindGroupLayout& InLayout)
        : m_Textures(InLayout.TextureSlotCount, nullptr)
    {
    }

    void OpenGLBindGroup::SetUniformBuffer(IUniformBuffer& InUniformBuffer)
    {
        m_UniformBuffer = &InUniformBuffer;
    }

    void OpenGLBindGroup::SetTexture(Uint32 InSlot, ITexture2D& InTexture)
    {
        if (InSlot < m_Textures.size())
        {
            m_Textures[InSlot] = &InTexture;
        }
    }

    void OpenGLBindGroup::Bind() const
    {
        // UBO already bound to its binding point at construction (OpenGLUniformBuffer) — no-op here.
        for (Uint32 i = 0; i < static_cast<Uint32>(m_Textures.size()); ++i)
        {
            if (m_Textures[i]) { m_Textures[i]->Bind(i); }
        }
    }
}
