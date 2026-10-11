#include "RHI/OpenGL/OpenGLShader.h"

#include <algorithm>
#include <string>

#include <glm/gtc/type_ptr.hpp>

#include "Core/Log/Logger.h"
#include "RHI/OpenGL/GLSLPort.h"

#include <glad/glad.h>

namespace Opaax
{
    namespace
    {
        const char* StageName(const GLenum InStage) noexcept
        {
            return InStage == GL_VERTEX_SHADER ? "vertex" : "fragment";
        }

        /** The compiled stage, or 0 (logged). */
        GLuint CompileStage(const GLenum InStage, const std::string& InSource, const char* InShaderName)
        {
            const GLuint lShader = glCreateShader(InStage);
            const char*  lSource = InSource.c_str();
            glShaderSource(lShader, 1, &lSource, nullptr);
            glCompileShader(lShader);

            GLint lSuccess = 0;
            glGetShaderiv(lShader, GL_COMPILE_STATUS, &lSuccess);
            if (lSuccess == GL_FALSE)
            {
                GLint lLength = 0;
                glGetShaderiv(lShader, GL_INFO_LOG_LENGTH, &lLength);
                std::string lLog(static_cast<size_t>(std::max(lLength, 1)), '\0');
                glGetShaderInfoLog(lShader, std::max(lLength, 1), nullptr, lLog.data());

                OPAAX_ENGINE_LOG(Error, "OpenGLShader '{}': the {} stage does not compile:\n{}",
                                 InShaderName, StageName(InStage), lLog.c_str());
                glDeleteShader(lShader);
                return 0;
            }

            return lShader;
        }
    }

    OpenGLShader::OpenGLShader(const ShaderDesc& InDesc)
        : m_DebugName(InDesc.DebugName)
    {
        CompileAndLink(InDesc);
    }

    OpenGLShader::~OpenGLShader()
    {
        glDeleteProgram(m_RendererID);
    }

    Int32 OpenGLShader::GetUniformLocation(const char* InName)
    {
        auto lIt = m_UniformLocationCache.find(InName);
        if (lIt != m_UniformLocationCache.end())
        {
            return lIt->second;
        }

        const Int32 lLocation = glGetUniformLocation(m_RendererID, InName);
        if (lLocation == -1)
        {
            OPAAX_ENGINE_LOG(Warn, "OpenGLShader '{}': uniform '{}' not found.", m_DebugName.CStr(), InName);
        }

        m_UniformLocationCache[InName] = lLocation;
        return lLocation;
    }

    void OpenGLShader::CompileAndLink(const ShaderDesc& InDesc)
    {
        const char* lName = m_DebugName.IsEmpty() ? "Shader" : m_DebugName.CStr();

        // Written for GLSL 4.50 with explicit bindings; run as 4.10 core, which macOS offers too.
        TDynArray<GLSLPort::ResourceBinding> lBindings;
        TDynArray<GLSLPort::ResourceBinding> lFragmentBindings;
        const std::string lVertexSrc   = GLSLPort::To410(InDesc.VertexSrc.CStr(), lBindings);
        const std::string lFragmentSrc = GLSLPort::To410(InDesc.FragmentSrc.CStr(), lFragmentBindings);
        lBindings.insert(lBindings.end(), lFragmentBindings.begin(), lFragmentBindings.end());

        const GLuint lVertexShader   = CompileStage(GL_VERTEX_SHADER, lVertexSrc, lName);
        const GLuint lFragmentShader = (lVertexShader != 0) ? CompileStage(GL_FRAGMENT_SHADER, lFragmentSrc, lName) : 0;
        if (lVertexShader == 0 || lFragmentShader == 0)
        {
            if (lVertexShader != 0) { glDeleteShader(lVertexShader); }
            OPAAX_CORE_ASSERT(false)
            return;
        }

        m_RendererID = glCreateProgram();
        glAttachShader(m_RendererID, lVertexShader);
        glAttachShader(m_RendererID, lFragmentShader);
        glLinkProgram(m_RendererID);

        glDetachShader(m_RendererID, lVertexShader);
        glDetachShader(m_RendererID, lFragmentShader);
        glDeleteShader(lVertexShader);
        glDeleteShader(lFragmentShader);

        GLint lSuccess = 0;
        glGetProgramiv(m_RendererID, GL_LINK_STATUS, &lSuccess);
        if (lSuccess == GL_FALSE)
        {
            GLint lLength = 0;
            glGetProgramiv(m_RendererID, GL_INFO_LOG_LENGTH, &lLength);
            std::string lLog(static_cast<size_t>(std::max(lLength, 1)), '\0');
            glGetProgramInfoLog(m_RendererID, std::max(lLength, 1), nullptr, lLog.data());

            OPAAX_ENGINE_LOG(Error, "OpenGLShader '{}': the program does not link:\n{}", lName, lLog.c_str());
            glDeleteProgram(m_RendererID);
            m_RendererID = 0;
            OPAAX_CORE_ASSERT(false)
            return;
        }

        // The bindings GLSL 4.10 cannot declare in the source.
        glUseProgram(m_RendererID);
        for (const GLSLPort::ResourceBinding& lBinding : lBindings)
        {
            if (lBinding.bBlock)
            {
                const GLuint lIndex = glGetUniformBlockIndex(m_RendererID, lBinding.Name.c_str());
                if (lIndex != GL_INVALID_INDEX)
                {
                    glUniformBlockBinding(m_RendererID, lIndex, static_cast<GLuint>(lBinding.Binding));
                }
                continue;
            }

            const GLint lLocation = glGetUniformLocation(m_RendererID, lBinding.Name.c_str());
            if (lLocation != -1)
            {
                TDynArray<GLint> lUnits(static_cast<size_t>(lBinding.ArraySize));
                for (Int32 lIndex = 0; lIndex < lBinding.ArraySize; ++lIndex)
                {
                    lUnits[static_cast<size_t>(lIndex)] = lBinding.Binding + lIndex;
                }
                glUniform1iv(lLocation, lBinding.ArraySize, lUnits.data());
            }
        }
        glUseProgram(0);
    }

    void OpenGLShader::Bind() const
    {
        glUseProgram(m_RendererID);
    }

    void OpenGLShader::Unbind() const
    {
        glUseProgram(0);
    }

    void OpenGLShader::SetInt(const char* InName, Int32 InValue)
    {
        glUniform1i(GetUniformLocation(InName), InValue);
    }

    void OpenGLShader::SetIntArray(const char* InName, const Int32* InValues, Uint32 InCount)
    {
        glUniform1iv(GetUniformLocation(InName), static_cast<GLsizei>(InCount), InValues);
    }

    void OpenGLShader::SetFloat(const char* InName, float InValue)
    {
        glUniform1f(GetUniformLocation(InName), InValue);
    }

    void OpenGLShader::SetFloat2(const char* InName, const Vector2F& InValue)
    {
        glUniform2f(GetUniformLocation(InName), InValue.x, InValue.y);
    }

    void OpenGLShader::SetFloat3(const char* InName, const Vector3F& InValue)
    {
        glUniform3f(GetUniformLocation(InName), InValue.x, InValue.y, InValue.z);
    }

    void OpenGLShader::SetFloat4(const char* InName, const Vector4F& InValue)
    {
        glUniform4f(GetUniformLocation(InName), InValue.x, InValue.y, InValue.z, InValue.w);
    }

    void OpenGLShader::SetMat4(const char* InName, const Matrix44F& InValue)
    {
        glUniformMatrix4fv(GetUniformLocation(InName), 1, GL_FALSE, glm::value_ptr(InValue));
    }
}
