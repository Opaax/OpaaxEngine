#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
 
namespace Opaax
{
    class ICommandBuffer;
    class IGraphicsContext;

    /**
     * @interface IRenderAPI
     *
     * Thin abstraction over the device + frame lifecycle of a graphics backend
     * (OpenGL, Vulkan, etc.). Nobody outside RHI/ calls backend functions directly.
     *
     * Owns the frame's command buffer: BeginFrame opens it, EndFrame submits it,
     * GetCommandBuffer hands it to passes + Renderer2D for the duration of the frame.
     * All clear/draw/bind work is recorded through ICommandBuffer — this interface no
     * longer carries per-draw state (it moved there with the command-recording reshape).
     *
     * Resource creation (buffers, shaders, textures, pipelines) lives in the concrete
     * resource classes via their own Create factories — they know their own API.
     */
    class OPAAX_API IRenderAPI
    {
    public:
        virtual ~IRenderAPI() = default;

        /**
         * Bring the device up against the already-initialized graphics context.
         * OpenGL ignores the context (GL state is global). Vulkan borrows the shared
         * VulkanDevice + swapchain the context owns (it acquires images + submits; the
         * context presents) — see VulkanRenderAPI.
         */
        virtual void Init(IGraphicsContext& InContext) = 0;

        /**
         * Per-frame begin/end bracket around all draw submission for the frame.
         * OpenGL: resets the frame's (immediate-executing) command buffer. An explicit
         * backend (Vulkan) acquires the swapchain image + begins recording (BeginFrame),
         * then ends + submits (EndFrame). Present itself is the graphics context's job
         * (IGraphicsContext::SwapBuffers), not the render API's.
         */
        virtual void BeginFrame() = 0;
        virtual void EndFrame()   = 0;

        // The frame's recorder, valid between BeginFrame and EndFrame.
        virtual ICommandBuffer& GetCommandBuffer() = 0;

        // Global viewport set (window-resize path; per-pass viewport rides BeginRenderPass).
        virtual void SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height) = 0;
    };

} // namespace Opaax
