#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxMathTypes.h"
#include "Core/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // ShaderDesc
    // =============================================================================
    /**
     * @struct ShaderDesc
     *
     * Backend-neutral description of a shader program — the input to IShader::Create.
     * Today it carries per-stage GLSL source; the language is implicitly GLSL. A future
     * backend (Step 8) extends this with a language/bytecode field (e.g. SPIR-V blob)
     * WITHOUT changing the consumer call site.
     */
    struct ShaderDesc
    {
        OpaaxString DebugName;     // identification / log label
        OpaaxString VertexSrc;     // vertex stage source
        OpaaxString FragmentSrc;   // fragment stage source
    };

    /**
     * @interface IShader
     *
     * Backend-agnostic shader program. Consumers (ShaderAsset, Renderer2D) hold a
     * UniquePtr<IShader> and never name a concrete backend type. The concrete impl
     * is selected by IShader::Create, defined in the active backend's TU
     * (OpenGLShader.cpp today) — mirrors the IVertexArray/IVertexBuffer factory pattern.
     *
     * Shader SOURCE portability (GLSL vs SPIR-V vs HLSL) is a separate concern and is
     * NOT solved here — Create still takes GLSL strings. A future backend-neutral shader
     * system replaces the source path without changing this interface.
     */
    class OPAAX_API IShader
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IShader() = default;

        // =============================================================================
        // Factory
        // =============================================================================
    public:
        static UniquePtr<IShader> Create(const ShaderDesc& InDesc);

        // =============================================================================
        // Functions
        // =============================================================================
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
