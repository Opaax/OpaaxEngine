#pragma once

#include "RHI/ICommandBuffer.h"

namespace Opaax
{
    class IRenderTarget;

    /**
     * @class OpenGLCommandBuffer
     *
     * OpenGL ICommandBuffer. GL is immediate-mode, so every method executes its GL calls right
     * away (there is no record/submit step — OpenGLRenderAPI reuses one instance per frame).
     * BeginRenderPass binds the target's framebuffer + sets the viewport + optionally clears;
     * the bind + DrawIndexed calls translate straight to GL state-set + glDrawElements.
     */
    class OPAAX_API OpenGLCommandBuffer final : public ICommandBuffer
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        OpenGLCommandBuffer()           = default;
        ~OpenGLCommandBuffer() override = default;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin ICommandBuffer interface
    public:
        void BeginRenderPass(IRenderTarget& InTarget, ELoadOp InLoadOp, const Vector4F& InClearColor) override;
        void EndRenderPass() override;

        void SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height) override;

        void BindPipeline(IPipeline& InPipeline)          override;
        void BindBindGroup(IBindGroup& InBindGroup)       override;
        void BindVertexArray(IVertexArray& InVertexArray) override;

        void DrawIndexed(Uint32 InIndexCount) override;
        //~End ICommandBuffer interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        IRenderTarget* m_CurrentTarget = nullptr;   // bound between BeginRenderPass/EndRenderPass
    };
}
