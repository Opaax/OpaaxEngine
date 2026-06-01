#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
 
namespace Opaax
{
    /**
     * @interface IRenderAPI
     *
     * Thin abstraction over raw graphics API calls (OpenGL, Vulkan, etc.).
     * Nobody outside RHI/ calls GL functions directly — they go through here.
     *
     * This is intentionally minimal. It covers the draw path only.
     * Resource creation (buffers, shaders, textures) lives in the concrete
     * Resource classes — they know their own API. IRenderAPI is purely the
     * State-setting and draw-call interface.
     */
    class OPAAX_API IRenderAPI
    {
    public:
        virtual ~IRenderAPI() = default;

        virtual void Init()                                                         = 0;

        /**
         * Per-frame begin/end bracket around all draw submission for the frame.
         * OpenGL: near-empty (immediate-mode submission). An explicit backend (Vulkan)
         * uses these to acquire the swapchain image + begin recording (BeginFrame) and
         * end + submit the command buffer (EndFrame). Present itself is the graphics
         * context's job (IGraphicsContext::SwapBuffers), not the render API's.
         */
        virtual void BeginFrame()                                                   = 0;
        virtual void EndFrame()                                                     = 0;

        virtual void SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height)   = 0;
        virtual void SetClearColor(float Red, float Green, float Blue, float Alpha) = 0;
        virtual void Clear()                                                        = 0;

        /**
         * called by Renderer2D after binding VAO and shader
         * @param IndexCount 
         */
        virtual void DrawIndexed(Uint32 IndexCount) = 0;
    };
 
} // namespace Opaax
