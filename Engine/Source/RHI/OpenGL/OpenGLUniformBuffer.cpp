#include "RHI/OpenGL/OpenGLUniformBuffer.h"

#include <glad/glad.h>

namespace Opaax
{

    OpenGLUniformBuffer::OpenGLUniformBuffer(Uint32 InSize, Uint32 InBinding)
    {
        glCreateBuffers(1, &m_RendererID);
        // GL_DYNAMIC_DRAW: rewritten every frame.
        glNamedBufferData(m_RendererID, InSize, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, InBinding, m_RendererID);
    }

    OpenGLUniformBuffer::~OpenGLUniformBuffer()
    {
        glDeleteBuffers(1, &m_RendererID);
    }

    void OpenGLUniformBuffer::SetData(const void* InData, Uint32 InSize, Uint32 InOffset)
    {
        glNamedBufferSubData(m_RendererID, InOffset, InSize, InData);
    }

} // namespace Opaax
