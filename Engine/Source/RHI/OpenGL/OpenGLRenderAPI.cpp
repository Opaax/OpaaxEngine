#include "OpenGLRenderAPI.h"

#include "Core/Application/Services/ILogger.h"

#define GLAD_APIENTRY
#include <glad/glad.h>

namespace Opaax
{
    void OpenGLRenderAPI::Init(IGraphicsContext& /*InContext*/)
    {
        OPAAX_LOG(LogOpenGLRenderAPI, Info, "OpenGL Initialized")
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
        OPAAX_LOG(LogOpenGLRenderAPI, Info, "Set Viewport X:{} Y:{} Width:{} Height:{}", X, Y, Width, Height)
        glViewport(static_cast<GLint>(X), static_cast<GLint>(Y), static_cast<GLsizei>(Width), static_cast<GLsizei>(Height));
    }

    void OpenGLRenderAPI::WaitIdle()
    {
        // GL executes immediately; glFinish blocks until all prior commands complete.
        glFinish();
    }

} // namespace Opaax