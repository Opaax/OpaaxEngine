#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"   // OPAAX_ENUM_VALUES
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // EBackend
    // =============================================================================
    /**
     * Graphics backend. OpenGL is the only one the engine has: Vulkan is a name a config may
     * hold, and is run as OpenGL (ResolveSupportedBackend).
     */
    enum class EBackend
    {
        OpenGL,
        Vulkan
    };

    // =============================================================================
    // Backend names and availability
    // =============================================================================

    // Name for logs and the value written in the config.
    const char* ToString(EBackend InBackend) noexcept;

    /**
     * The backend that is used for a requested one: OpenGL for Vulkan, with a Warn line.
     */
    EBackend ResolveSupportedBackend(EBackend InRequested);
}

OPAAX_ENUM_VALUES(Opaax::EBackend, OpenGL, Vulkan)
