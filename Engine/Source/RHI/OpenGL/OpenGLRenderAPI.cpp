#include "OpenGLRenderAPI.h"

#include "Core/Log/OpaaxLog.h"
#include "RHI/RenderAPI.h"

#define GLAD_APIENTRY
#include <glad/glad.h>

namespace Opaax
{
    // =============================================================================
    // RenderAPI factory
    // =============================================================================
    UniquePtr<IRenderAPI> RenderAPI::Create(EBackend InBackend)
    {
        switch (InBackend)
        {
            case EBackend::OpenGL: return MakeUnique<OpenGLRenderAPI>();
        }

        OPAAX_CORE_ERROR("RenderAPI::Create — unknown backend; no IRenderAPI created.");
        return nullptr;
    }

    EBackend RenderAPI::BackendFromString(const OpaaxString& InName)
    {
        if (InName == "OpenGL") { return EBackend::OpenGL; }

        OPAAX_CORE_WARN("RenderAPI: unknown render backend '{}' — falling back to OpenGL.", InName);
        return EBackend::OpenGL;
    }

    const char* RenderAPI::BackendToString(EBackend InBackend) noexcept
    {
        switch (InBackend)
        {
            case EBackend::OpenGL: return "OpenGL";
        }
        return "Unknown";
    }

    void OpenGLRenderAPI::Init()
    {
        // Enable blending for transparent sprites
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
 
        // Log GL info once at startup
        OPAAX_CORE_INFO("OpenGL Renderer: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        OPAAX_CORE_INFO("OpenGL Version:  {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    }
 
    void OpenGLRenderAPI::SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height)
    {
        glViewport(static_cast<GLint>(X), static_cast<GLint>(Y),
                   static_cast<GLsizei>(Width), static_cast<GLsizei>(Height));
    }
 
    void OpenGLRenderAPI::SetClearColor(float Red, float Green, float Blue, float Alpha)
    {
        glClearColor(Red, Green, Blue, Alpha);
    }
 
    void OpenGLRenderAPI::Clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
 
    void OpenGLRenderAPI::DrawIndexed(Uint32 IndexCount)
    {
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(IndexCount), GL_UNSIGNED_INT, nullptr);
    }
 
} // namespace Opaax