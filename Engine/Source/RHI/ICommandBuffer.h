#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    class IRenderTarget;
    class IPipeline;
    class IBindGroup;
    class IVertexArray;

    // =============================================================================
    // ELoadOp
    // =============================================================================
    // What a render pass does with the existing colour: Clear writes ClearColor first;
    // Load keeps it (e.g. UI drawn on top).
    enum class ELoadOp
    {
        Clear,
        Load
    };

    // =============================================================================
    // ICommandBuffer
    // =============================================================================
    /**
     * Records one frame's draw work. On OpenGL each call runs immediately; on a command-buffer
     * backend (Vulkan) it appends to the command buffer. Only binds and draws; resources are
     * created by the device.
     */
    class ICommandBuffer
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
        // Selects the target, clears or keeps its colour, and sets the viewport to its size.
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

        /**
         * One triangle covering the target, with no vertex buffer: the vertex shader makes the
         * positions from the vertex index (0, 1, 2). For post-process passes.
         */
        virtual void DrawFullscreen() = 0;
    };

} // namespace Opaax
