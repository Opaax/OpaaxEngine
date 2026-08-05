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
        // Created via IRHIDevice::CreateVertexBuffer.
    public:
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
        // Created via IRHIDevice::CreateIndexBuffer.
    public:
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
        // Created via IRHIDevice::CreateVertexArray.
    public:
        virtual void Bind()   const = 0;
        virtual void Unbind() const = 0;
 
        //------------------------------------------------------------------------------
        
        virtual void AddVertexBuffer(TUniquePtr<IVertexBuffer> InVBO) = 0;
        virtual void SetIndexBuffer(TUniquePtr<IIndexBuffer>   InIBO) = 0;

        //------------------------------------------------------------------------------
        //Get - Set
        
        virtual const TDynArray<TUniquePtr<IVertexBuffer>>& GetVertexBuffers() const = 0;
        virtual const IIndexBuffer*                        GetIndexBuffer()   const = 0;
 
    };
}
