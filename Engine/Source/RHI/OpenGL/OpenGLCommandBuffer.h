#pragma once

#include "RHI/ICommandBuffer.h"

namespace Opaax
{
    class IRenderTarget;

    /**
     * OpenGL ICommandBuffer. Every call runs immediately (no record/submit).
     */
    class OpenGLCommandBuffer final : public ICommandBuffer
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        OpenGLCommandBuffer() = default;
        ~OpenGLCommandBuffer() override;

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
        void DrawFullscreen() override;
        //~End ICommandBuffer interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        IRenderTarget* m_CurrentTarget = nullptr;   // set between BeginRenderPass and EndRenderPass

        /** A core profile draws nothing without a vertex array: an empty one, for DrawFullscreen. */
        Uint32 m_EmptyVertexArray = 0;
    };
}
