#pragma once

#include "RHI/Buffer.h"

namespace Opaax
{
    /**
     * @class OpenGLVertexBuffer
     *
     * Implement IVertexBuffer for OpenGL
     */
    class OPAAX_API OpenGLVertexBuffer final : public IVertexBuffer
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        /**
         * Dynamic VBO — data uploaded later via SetData()
         * @param InSize 
         */
        explicit OpenGLVertexBuffer(Uint32 InSize);
        
        /**
         * Static VBO — data uploaded at construction
         * @param InVertices 
         * @param InSize 
         */
        OpenGLVertexBuffer(const float* InVertices, Uint32 InSize);
        ~OpenGLVertexBuffer() override;

        // =============================================================================
        // Copy - Delete
        // =============================================================================
    public:
        OpenGLVertexBuffer(const OpenGLVertexBuffer&)            = delete;
        OpenGLVertexBuffer& operator=(const OpenGLVertexBuffer&) = delete;

        // =============================================================================
        // Move
        // =============================================================================
    public:
        OpenGLVertexBuffer(OpenGLVertexBuffer&&)                 = default;
        OpenGLVertexBuffer& operator=(OpenGLVertexBuffer&&)      = default;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IVertexBuffer interface
    public:
        void Bind()   const override;
        void Unbind() const override;

        //------------------------------------------------------------------------------
        //Get - Set
        
        void                SetData(const void* InData, Uint32 InSize)          override;
        void                SetLayout(const BufferLayout& InLayout)             override { m_Layout = InLayout; }
        const BufferLayout& GetLayout()                                 const   override { return m_Layout; }
        //~End IVertexBuffer interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint32       m_RendererID = 0;
        BufferLayout m_Layout;
    };
    
    /**
     * @class OpenGLIndexBuffer
     *
     * Implement IIndexBuffer for OpenGL
     */
    class OPAAX_API OpenGLIndexBuffer final : public IIndexBuffer
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        OpenGLIndexBuffer(const Uint32* InIndices, Uint32 InCount);
        ~OpenGLIndexBuffer() override;

        // =============================================================================
        // Copy - Delete
        // =============================================================================
    public:
        OpenGLIndexBuffer(const OpenGLIndexBuffer&)            = delete;
        OpenGLIndexBuffer& operator=(const OpenGLIndexBuffer&) = delete;

        // =============================================================================
        // Move
        // =============================================================================
    public:
        OpenGLIndexBuffer(OpenGLIndexBuffer&&)                 = default;
        OpenGLIndexBuffer& operator=(OpenGLIndexBuffer&&)      = default;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IIndexBuffer interface
    public:
        void   Bind()     const override;
        void   Unbind()   const override;

        //------------------------------------------------------------------------------
        //Get - Set
        FORCEINLINE Uint32 GetCount() const override { return m_Count; }
        //~End IIndexBuffer interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint32 m_RendererID = 0;
        Uint32 m_Count      = 0;
    };

} // namespace Opaax