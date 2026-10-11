#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // ShaderDesc
    // =============================================================================
    /**
     * A shader program description: per-stage GLSL source.
     */
    struct ShaderDesc
    {
        OpaaxString DebugName;     // identification / log label
        OpaaxString VertexSrc;     // vertex stage GLSL
        OpaaxString FragmentSrc;   // fragment stage GLSL
    };

    /**
     * Shader program. Created by IRHIDevice::CreateShader.
     */
    class IShader
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IShader() = default;

        // =============================================================================
        // Functions
        // =============================================================================
        // Created via IRHIDevice::CreateShader.
    public:
        virtual void Bind()   const = 0;
        virtual void Unbind() const = 0;

        //------------------------------------------------------------------------------
        // Uniform setters

        virtual void SetInt      (const char* InName, Int32           InValue)                  = 0;
        virtual void SetIntArray (const char* InName, const Int32*    InValues, Uint32 InCount) = 0;
        virtual void SetFloat    (const char* InName, float           InValue)                  = 0;
        virtual void SetFloat2   (const char* InName, const Vector2F&  InValue)                 = 0;
        virtual void SetFloat3   (const char* InName, const Vector3F&  InValue)                 = 0;
        virtual void SetFloat4   (const char* InName, const Vector4F&  InValue)                 = 0;
        virtual void SetMat4     (const char* InName, const Matrix44F& InValue)                 = 0;
    };
}
