#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    /**
     * @Enum EShaderDataType
     * 
     * Describes a single element in a vertex buffer layout
     */
    enum class EShaderDataType : Uint8
    {
        None = 0,
        Float, Float2, Float3, Float4,
        Int,   Int2,   Int3,   Int4,
        Bool,
        Mat3, Mat4,
    };
 
    static Uint32 ShaderDataTypeSize(EShaderDataType Type)
    {
        switch (Type)
        {
        case EShaderDataType::Float:  return 4;
        case EShaderDataType::Float2: return 4 * 2;
        case EShaderDataType::Float3: return 4 * 3;
        case EShaderDataType::Float4: return 4 * 4;
        case EShaderDataType::Int:    return 4;
        case EShaderDataType::Int2:   return 4 * 2;
        case EShaderDataType::Int3:   return 4 * 3;
        case EShaderDataType::Int4:   return 4 * 4;
        case EShaderDataType::Bool:   return 1;
        case EShaderDataType::Mat3:   return 4 * 3 * 3;
        case EShaderDataType::Mat4:   return 4 * 4 * 4;
        case EShaderDataType::None:
        default: return 0;
        }
    }
    
    /**
     * @Struct BufferElement
     *
     * One attribute in a BufferLayout
     */
    struct OPAAX_API BufferElement
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        BufferElement(EShaderDataType InType, bool InNormalized = false)
            : Type(InType)
            , Size(ShaderDataTypeSize(InType))
            , bNormalized(InNormalized)
        {}
 
        Uint32 GetComponentCount() const noexcept
        {
            switch (Type)
            {
            case EShaderDataType::Float:  return 1;
            case EShaderDataType::Float2: return 2;
            case EShaderDataType::Float3: return 3;
            case EShaderDataType::Float4: return 4;
            case EShaderDataType::Int:    return 1;
            case EShaderDataType::Int2:   return 2;
            case EShaderDataType::Int3:   return 3;
            case EShaderDataType::Int4:   return 4;
            case EShaderDataType::Bool:   return 1;
            case EShaderDataType::Mat3:   return 3;
            case EShaderDataType::Mat4:   return 4;
            case EShaderDataType::None:
            default: return 0;
            }
        }

        // =============================================================================
        // Members
        // =============================================================================
    public:
        EShaderDataType Type;
        Uint32          Size;
        Uint32          Offset      = 0;
        bool            bNormalized = false;
    };
}
