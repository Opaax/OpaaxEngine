#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

#include "RHI/RHIBackend.h"   // EBackend

namespace Opaax
{
    class IRHIDevice;
    class IGraphicsContext;

    // =============================================================================
    // RHIDevice — creates the IRHIDevice for an EBackend and starts it on the surface.
    // =============================================================================
    class RHIDevice
    {
    public:
        static TUniquePtr<IRHIDevice> Create(EBackend InBackend, IGraphicsContext& InSurface);
    };
}
