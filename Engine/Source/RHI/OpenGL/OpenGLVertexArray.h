#pragma once

#include "RHI/Buffer.h"

namespace Opaax
{
    class OPAAX_API OpenGLVertexArray final : public IVertexArray
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        OpenGLVertexArray();
        ~OpenGLVertexArray() override;

        // =============================================================================
        // Copy - delete
        // =============================================================================
    public:        
        OpenGLVertexArray(const OpenGLVertexArray&)             = delete;
        OpenGLVertexArray& operator=(const OpenGLVertexArray&)  = delete;

        // =============================================================================
        // Move
        // =============================================================================
    public:  
        OpenGLVertexArray(OpenGLVertexArray&&)              = default;
        OpenGLVertexArray& operator=(OpenGLVertexArray&&)   = default;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IVertexArray interface
    public:
        void Bind() const override;
        void Unbind() const override;

        void AddVertexBuffer(UniquePtr<IVertexBuffer> InVBO)    override;
        void SetIndexBuffer(UniquePtr<IIndexBuffer> InIBO)      override;

        //------------------------------------------------------------------------------
        //Get - Set
        
        const TDynArray<UniquePtr<IVertexBuffer>>&  GetVertexBuffers()      const override { return m_VertexBuffers; }
        const IIndexBuffer*                         GetIndexBuffer()        const override { return m_IndexBuffer.get(); }
        //~Begin IVertexArray interface

        // =============================================================================
        // Member
        // =============================================================================
    private:
        Uint32 m_RendererID = 0;
        Uint32 m_VBOIndex   = 0; // tracks attribute index across multiple VBOs

        TDynArray<UniquePtr<IVertexBuffer>> m_VertexBuffers;
        UniquePtr<IIndexBuffer>             m_IndexBuffer;
    };
}
