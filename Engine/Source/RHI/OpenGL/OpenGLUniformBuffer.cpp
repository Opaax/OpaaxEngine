#include "RHI/OpenGL/OpenGLUniformBuffer.h"

#include <glad/glad.h>

namespace Opaax
{

    OpenGLUniformBuffer::OpenGLUniformBuffer(Uint32 InSize, Uint32 InBinding)
        : m_Binding(InBinding)
    {
        glGenBuffers(1, &m_RendererID);
        glBindBuffer(GL_UNIFORM_BUFFER, m_RendererID);
        // GL_DYNAMIC_DRAW: rewritten every frame.
        glBufferData(GL_UNIFORM_BUFFER, InSize, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void OpenGLUniformBuffer::Bind() const
    {
        glBindBufferBase(GL_UNIFORM_BUFFER, m_Binding, m_RendererID);
    }

    OpenGLUniformBuffer::~OpenGLUniformBuffer()
    {
        glDeleteBuffers(1, &m_RendererID);
    }

    void OpenGLUniformBuffer::SetData(const void* InData, Uint32 InSize, Uint32 InOffset)
    {
        glBindBuffer(GL_UNIFORM_BUFFER, m_RendererID);
        glBufferSubData(GL_UNIFORM_BUFFER, InOffset, InSize, InData);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

} // namespace Opaax
