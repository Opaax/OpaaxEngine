#pragma once

#include "Core/EngineAPI.h"              // OPAAX_API
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"   // OPAAX_ENUM_VALUES
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // EBackend
    // =============================================================================
    /**
     * Graphics backend. Only OpenGL exists for now; Vulkan falls back to OpenGL.
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
    OPAAX_API const char* ToString(EBackend InBackend) noexcept;

    /**
     * The backend that will actually be used for a requested one (Vulkan -> OpenGL for now), logged.
     */
    OPAAX_API EBackend ResolveSupportedBackend(EBackend InRequested);
}

OPAAX_ENUM_VALUES(Opaax::EBackend, OpenGL, Vulkan)
