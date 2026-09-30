#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "RHI/RHIBackend.h"   // EBackend
#include "RHI/Shader.h"      // ShaderDesc

namespace Opaax
{
    class IGraphicsContext;

    // =============================================================================
    // RenderLimits — batch sizes. Given at Init.
    // =============================================================================
    struct RenderLimits
    {
        Uint32 MaxQuads        = 1000;
        Uint32 MaxTextureSlots = 16;
    };

    // =============================================================================
    // RenderSystemDesc — everything the RenderSystem needs at startup, built by the host
    //   (config, window, shader source). Surface is the graphics context (make-current,
    //   present, vsync). SpriteShader is source text the host read from disk.
    // =============================================================================
    struct RenderSystemDesc
    {
        EBackend          Backend = EBackend::OpenGL;
        IGraphicsContext* Surface = nullptr;
        Uint32            Width   = 0;
        Uint32            Height  = 0;
        RenderLimits      Limits;
        ShaderDesc        SpriteShader;
        Vector4F          ClearColor{0.f, 0.f, 0.f, 1.f};
    };
}
