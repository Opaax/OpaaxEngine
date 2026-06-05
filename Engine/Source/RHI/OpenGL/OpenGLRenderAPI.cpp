#include "OpenGLRenderAPI.h"

#include "Core/Log/OpaaxLog.h"

#define GLAD_APIENTRY
#include <glad/glad.h>

namespace Opaax
{
    // NOTE: the RenderAPI / resource Create factory dispatch lives in RHI/BackendFactory.cpp
    //   (the one neutral TU that knows every backend). This file holds only the GL impl.

    void OpenGLRenderAPI::Init(IGraphicsContext& /*InContext*/)
    {
        // OpenGL state is global — the context (already make-current'd) is not needed here.
        // Blend is pipeline state now (set on BindPipeline) — not a global default here.

        // Log GL info once at startup
        OPAAX_CORE_INFO("OpenGL Renderer: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        OPAAX_CORE_INFO("OpenGL Version:  {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    }

    void OpenGLRenderAPI::BeginFrame()
    {
        // OpenGL submits immediately to the current context — nothing to begin.
        // NOTE: Vulkan acquires the swapchain image + begins the command buffer here.
    }

    void OpenGLRenderAPI::EndFrame()
    {
        // OpenGL has nothing to flush before the context swap.
        // NOTE: Vulkan ends + submits the command buffer here (present stays in the context).
    }

    ICommandBuffer& OpenGLRenderAPI::GetCommandBuffer()
    {
        return m_CommandBuffer;
    }

    void OpenGLRenderAPI::SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height)
    {
        glViewport(static_cast<GLint>(X), static_cast<GLint>(Y),
                   static_cast<GLsizei>(Width), static_cast<GLsizei>(Height));
    }

    void OpenGLRenderAPI::WaitIdle()
    {
        // GL executes immediately; glFinish blocks until all prior commands complete.
        glFinish();
    }

} // namespace Opaax