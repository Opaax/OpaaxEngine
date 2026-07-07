#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxMathTypes.h"
#include "RHI/RenderLog.h"
#include "RHI/RenderAPI.h"   // EBackend
#include "RHI/Shader.h"      // ShaderDesc

namespace Opaax
{
    class IGraphicsContext;

    // =============================================================================
    // RenderLimits — batcher sizing knobs (POD). Handed in at Init.
    // =============================================================================
    struct RenderLimits
    {
        Uint32 MaxQuads        = 1000;
        Uint32 MaxTextureSlots = 16;
    };

    // =============================================================================
    // RenderSystemDesc — everything the portable RenderSystem needs at startup, as one
    //   descriptor (the WindowProps pattern). The host adapter builds it from its own
    //   world (config, window, paths); the RenderSystem never reaches back into the host.
    //   Surface is the already-created graphics context (make-current / present / vsync) —
    //   the module stays windowing-agnostic. SpriteShader is source the HOST read off disk
    //   (no IPaths / file IO inside the module).
    // =============================================================================
    struct RenderSystemDesc
    {
        EBackend          Backend = EBackend::OpenGL;
        IGraphicsContext* Surface = nullptr;
        Uint32            Width   = 0;
        Uint32            Height  = 0;
        RenderLimits      Limits;
        RenderLogFn       Log = &DefaultRenderLog;
        ShaderDesc        SpriteShader;
        Vector4F          ClearColor{0.f, 0.f, 0.f, 1.f};
    };
}
