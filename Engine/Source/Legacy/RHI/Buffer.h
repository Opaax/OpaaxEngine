#pragma once

#include "ShaderDataTypes.h"

namespace Opaax
{
    // =============================================================================
    // BufferLayout — describes the full vertex layout to the GPU
    //
    // Usage:
    //   BufferLayout lLayout = {
    //       { EShaderDataType::Float3 },  // position
    //       { EShaderDataType::Float4 },  // colour
    //       { EShaderDataType::Float2 },  // uv
    //       { EShaderDataType::Float  },  // texture index
    //   };
    // =============================================================================

    /**
     * @Class BufferLayout
     *
     * Describes the full vertex layout to the GPU
     *
     * BufferLayout lLayout = {
     * { EShaderDataType::Float3 },  // position
     * { EShaderDataType::Float4 },  // colour
     * { EShaderDataType::Float2 },  // uv
     * { EShaderDataType::Float  },  // texture index
     * };
     */
    class OPAAX_API BufferLayout
    {
        // =============================================================================
        // CTOR 
        // =============================================================================
    public:
        BufferLayout() = default;
        BufferLayout(TInitArray<BufferElement> InElements)
            : m_Elements(InElements)
        {
            CalculateOffsetsAndStride();
        }
        
        // =============================================================================
        // Functions 
        // =============================================================================
    private:
        void CalculateOffsetsAndStride();
        
    public:

        //------------------------------------------------------------------------------
        //Get - Set
        
        FORCEINLINE Uint32                          GetStride()     const noexcept { return m_Stride; }
        FORCEINLINE const TDynArray<BufferElement>& GetElements()   const noexcept { return m_Elements; }

        // =============================================================================
        // Members 
        // =============================================================================
    private:
        TDynArray<BufferElement>    m_Elements;
        Uint32                      m_Stride = 0;
    };
    
    /**
     * @class IVertexBuffer
     */
    class OPAAX_API IVertexBuffer
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        virtual ~IVertexBuffer() = default;

        // =============================================================================
        // Functions
        // =============================================================================
        
        //------------------------------------------------------------------------------
        //Static
    public:
        static UniquePtr<IVertexBuffer> Create(Uint32 InSize); // dynamic
        static UniquePtr<IVertexBuffer> Create(const float* InVertices, Uint32 InSize); // static
        
        //------------------------------------------------------------------------------
        
        virtual void Bind()     const = 0;
        virtual void Unbind()   const = 0;

        //------------------------------------------------------------------------------
        //Get - Set
        
        /**
         * Upload new data into a dynamic VBO (used by Renderer2D batch flush)
         * @param InData 
         * @param InSize 
         */
        virtual void SetData(const void* InData, Uint32 InSize) = 0;
        virtual void SetLayout(const BufferLayout& InLayout)    = 0;
        
        virtual const BufferLayout& GetLayout() const = 0;
    };
    
    /**
     * @Class IIndexBuffer
     */
    class OPAAX_API IIndexBuffer
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        virtual ~IIndexBuffer() = default;

        // =============================================================================
        // Functions
        // =============================================================================

    public:
        //------------------------------------------------------------------------------
        //Static
        static UniquePtr<IIndexBuffer> Create(const Uint32* InIndices, Uint32 InCount);
        
        //------------------------------------------------------------------------------
        
        virtual void Bind()     const = 0;
        virtual void Unbind()   const = 0;

        //------------------------------------------------------------------------------
        //Get - Set
        
        virtual Uint32 GetCount() const = 0;
    };

    /**
     * @class IVertexArray
     */
    class OPAAX_API IVertexArray
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        virtual ~IVertexArray() = default;

        // =============================================================================
        // Functions
        // =============================================================================

        //------------------------------------------------------------------------------
        //Static
    public:
        static UniquePtr<IVertexArray> Create();
        
    public: 
        virtual void Bind()   const = 0;
        virtual void Unbind() const = 0;
 
        //------------------------------------------------------------------------------
        
        virtual void AddVertexBuffer(UniquePtr<IVertexBuffer> InVBO) = 0;
        virtual void SetIndexBuffer(UniquePtr<IIndexBuffer>   InIBO) = 0;

        //------------------------------------------------------------------------------
        //Get - Set
        
        virtual const TDynArray<UniquePtr<IVertexBuffer>>& GetVertexBuffers() const = 0;
        virtual const IIndexBuffer*                        GetIndexBuffer()   const = 0;
 
    };
}
