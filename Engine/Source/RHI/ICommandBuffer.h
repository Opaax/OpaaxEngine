#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxMathTypes.h"

namespace Opaax
{
    class IRenderTarget;
    class IPipeline;
    class IBindGroup;
    class IVertexArray;

    // =============================================================================
    // ELoadOp
    // =============================================================================
    // Color-attachment behavior at BeginRenderPass. Clear writes ClearColor first;
    // Load keeps existing contents (composite-on-top passes like the overlay).
    enum class ELoadOp
    {
        Clear,
        Load
    };

    // =============================================================================
    // ICommandBuffer
    // =============================================================================
    /**
     * @interface ICommandBuffer
     *
     * Backend-neutral recorder for one frame's draw work. The render API owns the
     * frame's command buffer (BeginFrame opens it, EndFrame submits it); passes and
     * Renderer2D record into it via this interface. On OpenGL each call executes
     * immediately (GL is immediate-mode); on a command-buffer backend (Vulkan) each
     * call appends to the live VkCommandBuffer.
     *
     * Render targets, pipelines, bind groups and vertex arrays are created through
     * their own factories; the command buffer only binds + draws with them.
     */
    class OPAAX_API ICommandBuffer
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~ICommandBuffer() = default;

        // =============================================================================
        // Render pass bracket
        // =============================================================================
    public:
        // Select the target for subsequent draws; clear or load its color. Sets the
        // viewport to the target's full size.
        virtual void BeginRenderPass(IRenderTarget& InTarget, ELoadOp InLoadOp, const Vector4F& InClearColor) = 0;
        virtual void EndRenderPass() = 0;

        // =============================================================================
        // State + draw
        // =============================================================================
    public:
        virtual void SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height) = 0;

        virtual void BindPipeline(IPipeline& InPipeline)       = 0;
        virtual void BindBindGroup(IBindGroup& InBindGroup)    = 0;
        virtual void BindVertexArray(IVertexArray& InVertexArray) = 0;

        virtual void DrawIndexed(Uint32 InIndexCount) = 0;
    };

} // namespace Opaax
