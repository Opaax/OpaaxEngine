#pragma once

#include "Core/EngineAPI.h"
#include "Platform/IPlatform.h"
#include "Platform/IFileSystem.h"

#ifdef OPAAX_PLATFORM_WINDOWS

#include "Platform/Windows/WindowsFileSystem.h"

namespace Opaax
{
    // =============================================================================
    // WindowsPlatform — IPlatform using Win32.
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
        // Concrete type, by value; exposed as const IFileSystem&.
        WindowsFileSystem m_FileSystem;
    };
}

#endif // OPAAX_PLATFORM_WINDOWS
