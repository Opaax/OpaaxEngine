#pragma once

#include "Core/EngineAPI.h"
#include "Core/Hash/OpaaxHash.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/OpaaxTypes.h"

#include "Core/Maths/MathTypes.h"
#include "RHI/Shader.h"

namespace Opaax
{
    /**
     * OpenGL IShader. Uses SPIR-V (GL_ARB_gl_spirv) when available, else compiles the GLSL source.
     * The name-based uniform setters do nothing on a SPIR-V program (no default-block uniforms).
     */
    class OPAAX_API OpenGLShader final : public IShader
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        explicit OpenGLShader(const ShaderDesc& InDesc);
        ~OpenGLShader();

        // =============================================================================
        // Copy - delete
        // =============================================================================
        OpenGLShader(const OpenGLShader&)            = delete;
        OpenGLShader& operator=(const OpenGLShader&) = delete;

        // =============================================================================
        // Move
        // =============================================================================
        OpenGLShader(OpenGLShader&&)                 = default;
        OpenGLShader& operator=(OpenGLShader&&)      = default;

        // =============================================================================
        // Function
        // =============================================================================
    private:
        Int32 GetUniformLocation(const char* InName);
        void  CreateFromSpirv(const TDynArray<Uint32>& InVertexSpirv, const TDynArray<Uint32>& InFragmentSpirv);
        void  CompileAndLink(const char* InVertexSrc, const char* InFragmentSrc);  // GLSL fallback
        
        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IShader interface
    public:
        void Bind()   const override;
        void Unbind() const override;

        //------------------------------------------------------------------------------
        //  Uniform setters

        void SetInt         (const char* InName, Int32              InValue                 ) override;
        void SetIntArray    (const char* InName, const Int32*       InValues, Uint32 InCount) override;
        void SetFloat       (const char* InName, float              InValue                 ) override;
        void SetFloat2      (const char* InName, const Vector2F&    InValue                 ) override;
        void SetFloat3      (const char* InName, const Vector3F&    InValue                 ) override;
        void SetFloat4      (const char* InName, const Vector4F&    InValue                 ) override;
        void SetMat4        (const char* InName, const Matrix44F&   InValue                 ) override;
        //~End IShader interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint32 m_RendererID = 0;

        // Uniform lookup by name (per draw call, not per vertex).
        TUnorderedMap<OpaaxString, Int32, OpaaxHash> m_UniformLocationCache;
    };
}
