#pragma once

#include "Core/EngineAPI.h"

#if defined(OPAAX_PLATFORM_WINDOWS)
#include "Platform/Windows/WindowsPlatform.h"
#elif defined(OPAAX_PLATFORM_POSIX)
#include "Platform/Posix/PosixPlatform.h"
#endif

namespace Opaax
{
    // =============================================================================
    // NativePlatform — the IPlatform of the platform being compiled for.
    // =============================================================================
#if defined(OPAAX_PLATFORM_WINDOWS)
    using NativePlatform = WindowsPlatform;
#elif defined(OPAAX_PLATFORM_POSIX)
    using NativePlatform = PosixPlatform;
#endif
}
