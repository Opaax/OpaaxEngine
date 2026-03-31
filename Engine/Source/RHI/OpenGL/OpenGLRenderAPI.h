#pragma once

#include "RHI/IRenderAPI.h"

namespace Opaax
{
    /**
     * @class OpenGLRenderAPI
     *
     * Implement IRenderAPI for OpenGL
     */
    class OPAAX_API OpenGLRenderAPI final : public IRenderAPI
    {
        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin IRenderAPI interface
    public:
        void Init()                                                         override;
        void SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height)   override;
        void SetClearColor(float Red, float Green, float Blue, float Alpha) override;
        void Clear()                                                        override;
        void DrawIndexed(Uint32 IndexCount)                                 override;
        //~Begin IRenderAPI interface
    };
} // namespace Opaax
