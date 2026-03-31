#include "OpenGLShader.h"
#include <glm/gtc/type_ptr.hpp>

#include "Core/Log/OpaaxLog.h"

#define GLAD_APIENTRY
#include <glad/glad.h>

namespace Opaax
{
    OpenGLShader::OpenGLShader(const char* InVertexSrc, const char* InFragmentSrc)
    {
        CompileAndLink(InVertexSrc, InFragmentSrc);
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
            OPAAX_CORE_WARN("OpenGLShader: uniform '{}' not found in shader.", InName);
        }
 
        m_UniformLocationCache[InName] = lLocation;
        
        return lLocation;
    }
    
    void OpenGLShader::CompileAndLink(const char* InVertexSrc, const char* InFragmentSrc)
    {
        // Compile vertex shader
        const GLuint lVertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(lVertexShader, 1, &InVertexSrc, nullptr);
        glCompileShader(lVertexShader);
 
        GLint lSuccess = 0;
        glGetShaderiv(lVertexShader, GL_COMPILE_STATUS, &lSuccess);
        if (!lSuccess)
        {
            char lLog[512];
            glGetShaderInfoLog(lVertexShader, 512, nullptr, lLog);
            OPAAX_CORE_ERROR("OpenGLShader: vertex shader compilation failed:\n{}", lLog);
            glDeleteShader(lVertexShader);
            OPAAX_CORE_ASSERT(false)
            return;
        }
 
        // Compile fragment shader
        const GLuint lFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(lFragmentShader, 1, &InFragmentSrc, nullptr);
        glCompileShader(lFragmentShader);
 
        glGetShaderiv(lFragmentShader, GL_COMPILE_STATUS, &lSuccess);
        if (!lSuccess)
        {
            char lLog[512];
            glGetShaderInfoLog(lFragmentShader, 512, nullptr, lLog);
            OPAAX_CORE_ERROR("OpenGLShader: fragment shader compilation failed:\n{}", lLog);
            glDeleteShader(lVertexShader);
            glDeleteShader(lFragmentShader);
            OPAAX_CORE_ASSERT(false)
            return;
        }
 
        // Link
        m_RendererID = glCreateProgram();
        glAttachShader(m_RendererID, lVertexShader);
        glAttachShader(m_RendererID, lFragmentShader);
        glLinkProgram(m_RendererID);
 
        glGetProgramiv(m_RendererID, GL_LINK_STATUS, &lSuccess);
        if (!lSuccess)
        {
            char lLog[512];
            glGetProgramInfoLog(m_RendererID, 512, nullptr, lLog);
            OPAAX_CORE_ERROR("OpenGLShader: program link failed:\n{}", lLog);
            glDeleteProgram(m_RendererID);
            m_RendererID = 0;
            OPAAX_CORE_ASSERT(false)
        }
 
        glDeleteShader(lVertexShader);
        glDeleteShader(lFragmentShader);
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

