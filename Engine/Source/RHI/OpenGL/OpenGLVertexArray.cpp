#include "OpenGLVertexArray.h"
#include "Core/Log/OpaaxLog.h"

#define GLAD_APIENTRY
#include <glad/glad.h>

namespace Opaax
{

    // =============================================================================
    // IVertexArray factory
    // =============================================================================
    UniquePtr<IVertexArray> IVertexArray::Create()
    {
        return MakeUnique<OpenGLVertexArray>();
    }
    
    OpenGLVertexArray::OpenGLVertexArray()
    {
        glCreateVertexArrays(1, &m_RendererID);
    }

    OpenGLVertexArray::~OpenGLVertexArray()
    {
        glDeleteVertexArrays(1, &m_RendererID);
    }

    void OpenGLVertexArray::Bind() const
    {
        glBindVertexArray(m_RendererID);
    }
    
    void OpenGLVertexArray::Unbind() const
    {
        glBindVertexArray(0);
    }
    
    void OpenGLVertexArray::AddVertexBuffer(UniquePtr<IVertexBuffer> InVBO)
    {
        OPAAX_CORE_ASSERT(!InVBO->GetLayout().GetElements().empty())
 
        glBindVertexArray(m_RendererID);
        InVBO->Bind();
 
        const auto& lLayout = InVBO->GetLayout();
        for (const auto& lElement : lLayout.GetElements())
        {
            switch (lElement.Type)
            {
            case EShaderDataType::Float:
            case EShaderDataType::Float2:
            case EShaderDataType::Float3:
            case EShaderDataType::Float4:
                {
                    glEnableVertexAttribArray(m_VBOIndex);
                    glVertexAttribPointer(
                        m_VBOIndex,
                        static_cast<GLint>(lElement.GetComponentCount()),
                        GL_FLOAT,
                        lElement.bNormalized ? GL_TRUE : GL_FALSE,
                        static_cast<GLsizei>(lLayout.GetStride()),
                        reinterpret_cast<const void*>(static_cast<uintptr_t>(lElement.Offset)));
                    ++m_VBOIndex;
                    break;
                }
            case EShaderDataType::Int:
            case EShaderDataType::Int2:
            case EShaderDataType::Int3:
            case EShaderDataType::Int4:
            case EShaderDataType::Bool:
                {
                    glEnableVertexAttribArray(m_VBOIndex);
                    glVertexAttribIPointer(
                        m_VBOIndex,
                        static_cast<GLint>(lElement.GetComponentCount()),
                        GL_INT,
                        static_cast<GLsizei>(lLayout.GetStride()),
                        reinterpret_cast<const void*>(static_cast<uintptr_t>(lElement.Offset)));
                    ++m_VBOIndex;
                    break;
                }
            default:
                OPAAX_CORE_ASSERT(false) // Unhandled shader data type
                break;
            }
        }
 
        m_VertexBuffers.push_back(Move(InVBO));
    }
    
    void OpenGLVertexArray::SetIndexBuffer(UniquePtr<IIndexBuffer> InIBO)
    {
        glBindVertexArray(m_RendererID);
        InIBO->Bind();
        m_IndexBuffer = Move(InIBO);
    }
}

