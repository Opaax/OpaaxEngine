#pragma once

#include "IPlatform.h"
#include "Core/EngineAPI.h"

#ifdef OPAAX_PLATFORM_WINDOWS

namespace Opaax
{
    // =============================================================================
    // WindowsPlatform — IPlatform backed by Win32 (GetModuleFileNameW, etc.).
    // =============================================================================
    class OPAAX_API WindowsPlatform final : public IPlatform
    {
        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IPlatform interface
    public:
        Uint32      GetLogicalCoreCount() const override;
        double      GetTimeSeconds()      const override;
        OpaaxString GetExecutablePath()   const override;
        //~End IPlatform interface
    };
}

#endif // OPAAX_PLATFORM_WINDOWS
