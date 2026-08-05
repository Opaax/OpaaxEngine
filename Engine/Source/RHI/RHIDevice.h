#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

#include "RHI/RHIBackend.h"   // EBackend

namespace Opaax
{
    class IRHIDevice;
    class IGraphicsContext;

    // =============================================================================
    // RHIDevice — the device factory. The single place that maps EBackend -> a concrete
    //   IRHIDevice, creates it, and Init's it against the surface. Adding a backend =
    //   one case here + that backend's device impl.
    // =============================================================================
    class OPAAX_API RHIDevice
    {
    public:
        static TUniquePtr<IRHIDevice> Create(EBackend InBackend, IGraphicsContext& InSurface);
    };
}
