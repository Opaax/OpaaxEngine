#include "OpenGLBuffer.h"

#define GLAD_APIENTRY
#include <glad/glad.h>

namespace Opaax
{
    //------------------------------------------------------------------------------
    // OpenGLVertexBuffer

    // =============================================================================
    // IVertexBuffer factory
    // =============================================================================
    UniquePtr<IVertexBuffer> IVertexBuffer::Create(Uint32 InSize)
    {
        return MakeUnique<OpenGLVertexBuffer>(InSize);
    }
 
    UniquePtr<IVertexBuffer> IVertexBuffer::Create(const float* InVertices, Uint32 InSize)
    {
        return MakeUnique<OpenGLVertexBuffer>(InVertices, InSize);
    }

    OpenGLVertexBuffer::OpenGLVertexBuffer(Uint32 InSize)
    {
        glCreateBuffers(1, &m_RendererID);
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        // GL_DYNAMIC_DRAW — data changes every frame (batch vertex upload)
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
        // glBufferSubData — avoids reallocation, only updates content
        glBufferSubData(GL_ARRAY_BUFFER, 0, InSize, InData);
    }
    
    //------------------------------------------------------------------------------
    // OpenGLIndexBuffer

    // =============================================================================
    // IIndexBuffer factory
    // =============================================================================
    UniquePtr<IIndexBuffer> IIndexBuffer::Create(const Uint32* InIndices, Uint32 InCount)
    {
        return MakeUnique<OpenGLIndexBuffer>(InIndices, InCount);
    }
    
    OpenGLIndexBuffer::OpenGLIndexBuffer(const Uint32* InIndices, Uint32 InCount)
    {
        // NOTE: GL_ELEMENT_ARRAY_BUFFER must be bound to a VAO to be remembered.
        //   We bind here during construction — the VAO must already be bound.
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

