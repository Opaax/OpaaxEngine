#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxHash.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxTypes.h"

#include <glm/glm.hpp>

#include "Core/OpaaxMathTypes.h"
#include "RHI/Shader.h"

namespace Opaax
{
    /**
     *@class OpenGLShader
     *
     * OpenGL IShader implementation. Compiles and links a vertex + fragment GLSL shader pair.
     * Uniform locations are cached on first lookup — glGetUniformLocation is never called twice for the same name.
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
        void  CompileAndLink(const char* InVertexSrc, const char* InFragmentSrc);
        
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

        // OpaaxString keys + OpaaxHash — uniform lookup is not on the per-vertex
        //   hot path, only per draw call (batch flush). Per-call temporary
        //   OpaaxString construction from const char* is acceptable.
        UnorderedMap<OpaaxString, Int32, OpaaxHash> m_UniformLocationCache;
    };
}
