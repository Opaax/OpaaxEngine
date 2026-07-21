#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

#include "RHI/RenderAPI.h"   // EBackend
#include "RHI/RenderLog.h"   // RenderLogFn

namespace Opaax
{
    class IRHIDevice;
    class IGraphicsContext;

    // =============================================================================
    // RHIDevice — the device factory (mirrors RenderAPI::Create). The single place that
    //   maps EBackend -> a concrete IRHIDevice, creates it, and Init's it against the
    //   surface. Adding a backend = one case here + that backend's device impl.
    // =============================================================================
    class OPAAX_API RHIDevice
    {
    public:
        static UniquePtr<IRHIDevice> Create(EBackend InBackend, IGraphicsContext& InSurface, RenderLogFn InLog);
    };
}
