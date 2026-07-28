#pragma once

#include "Application/Services/Platforms/IFileSystem.h"
#include "Core/EngineAPI.h"
#include "Application/Services/Platforms/IPlatform.h"

#ifdef OPAAX_PLATFORM_WINDOWS

#include "Application/Services/Platforms/Windows/WindowsFileSystem.h"

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
        Uint32                              GetLogicalCoreCount()   const override;
        double                              GetTimeSeconds()        const override;
        OpaaxString                         GetExecutablePath()     const override;
        OpaaxString                         GetPlatformName()       const override { return OpaaxString("Windows"); }
        [[nodiscard]] const IFileSystem&    GetFileSystem()         const override { return m_FileSystem; }
        //~End IPlatform interface
        
        // =============================================================================
        // Members
        // =============================================================================
    private:
        // The platform's OWN concrete type, by value — the accessor narrows it to const IFileSystem&,
        // so no call site knows or cares which one it got.
        WindowsFileSystem m_FileSystem;
    };
}

#endif // OPAAX_PLATFORM_WINDOWS
