#include "OpenGLCommandBuffer.h"

#include "OpenGLPipeline.h"
#include "OpenGLBindGroup.h"
#include "RHI/Buffer.h"
#include "Renderer/RenderTarget.hpp"

#define GLAD_APIENTRY
#include <glad/glad.h>

namespace Opaax
{
    void OpenGLCommandBuffer::BeginRenderPass(IRenderTarget& InTarget, ELoadOp InLoadOp, const Vector4F& InClearColor)
    {
        m_CurrentTarget = &InTarget;
        InTarget.Bind();

        // Per-pass viewport = the target's full size (FBO bind also sets it, but the backbuffer
        // target relies on this).
        glViewport(0, 0, static_cast<GLsizei>(InTarget.GetWidth()), static_cast<GLsizei>(InTarget.GetHeight()));

        if (InLoadOp == ELoadOp::Clear)
        {
            glClearColor(InClearColor.r, InClearColor.g, InClearColor.b, InClearColor.a);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
    }

    void OpenGLCommandBuffer::EndRenderPass()
    {
        if (m_CurrentTarget)
        {
            m_CurrentTarget->Unbind();
            m_CurrentTarget = nullptr;
        }
    }

    void OpenGLCommandBuffer::SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height)
    {
        glViewport(static_cast<GLint>(X), static_cast<GLint>(Y),
                   static_cast<GLsizei>(Width), static_cast<GLsizei>(Height));
    }

    void OpenGLCommandBuffer::BindPipeline(IPipeline& InPipeline)
    {
        // Backend invariant: the active backend's factory only produces OpenGLPipeline here.
        static_cast<OpenGLPipeline&>(InPipeline).Apply();
    }

    void OpenGLCommandBuffer::BindBindGroup(IBindGroup& InBindGroup)
    {
        static_cast<OpenGLBindGroup&>(InBindGroup).Bind();
    }

    void OpenGLCommandBuffer::BindVertexArray(IVertexArray& InVertexArray)
    {
        InVertexArray.Bind();
    }

    void OpenGLCommandBuffer::DrawIndexed(Uint32 InIndexCount)
    {
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(InIndexCount), GL_UNSIGNED_INT, nullptr);
    }
}
