#pragma once

#include "RHI/IRenderAPI.h"
#include "RHI/OpenGL/OpenGLCommandBuffer.h"

namespace Opaax
{
    /**
     * @class OpenGLRenderAPI
     *
     * Implement IRenderAPI for OpenGL. Owns the frame's command buffer (a single immediate-
     * executing OpenGLCommandBuffer reused every frame — GL has no recording/submit step).
     */
    class OPAAX_API OpenGLRenderAPI final : public IRenderAPI
    {
        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin IRenderAPI interface
    public:
        void            Init()                                                       override;
        void            BeginFrame()                                                 override;
        void            EndFrame()                                                   override;
        ICommandBuffer& GetCommandBuffer()                                           override;
        void            SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height) override;
        //~End IRenderAPI interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpenGLCommandBuffer m_CommandBuffer;
    };
} // namespace Opaax
