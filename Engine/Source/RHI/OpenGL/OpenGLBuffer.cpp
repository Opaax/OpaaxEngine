#include "OpenGLBuffer.h"

#include <glad/glad.h>

namespace Opaax
{

    //------------------------------------------------------------------------------
    // OpenGLVertexBuffer

    OpenGLVertexBuffer::OpenGLVertexBuffer(Uint32 InSize)
    {
        glCreateBuffers(1, &m_RendererID);
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        // GL_DYNAMIC_DRAW: updated every frame.
        glBufferData(GL_ARRAY_BUFFER, InSize, nullptr, GL_DYNAMIC_DRAW);
    }
    
    OpenGLVertexBuffer::OpenGLVertexBuffer(const float* InVertices, Uint32 InSize)
    {
        glCreateBuffers(1, &m_RendererID);
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        glBufferData(GL_ARRAY_BUFFER, InSize, InVertices, GL_STATIC_DRAW);
    }

    OpenGLVertexBuffer::~OpenGLVertexBuffer()
    {
        glDeleteBuffers(1, &m_RendererID);
    }

    void OpenGLVertexBuffer::Bind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
    }
    
    void OpenGLVertexBuffer::Unbind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    
    void OpenGLVertexBuffer::SetData(const void* InData, Uint32 InSize)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        // glBufferSubData: updates without reallocating.
        glBufferSubData(GL_ARRAY_BUFFER, 0, InSize, InData);
    }
    
    //------------------------------------------------------------------------------
    // OpenGLIndexBuffer

    OpenGLIndexBuffer::OpenGLIndexBuffer(const Uint32* InIndices, Uint32 InCount)
    {
        // The element buffer binding is stored in the VAO: the VAO must be bound here.
        glCreateBuffers(1, &m_RendererID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(InCount * sizeof(Uint32)),
                     InIndices,
                     GL_STATIC_DRAW);
    }
    
    OpenGLIndexBuffer::~OpenGLIndexBuffer()
    {
        glDeleteBuffers(1, &m_RendererID);
    }
    
    void OpenGLIndexBuffer::Bind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
    }
    
    void OpenGLIndexBuffer::Unbind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

