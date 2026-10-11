#pragma once

#include "Core/EngineAPI.h"
#include "Platform/IPlatform.h"
#include "Platform/StdFileSystem.h"

#ifdef OPAAX_PLATFORM_POSIX

namespace Opaax
{
    // =============================================================================
    // PosixPlatform — IPlatform for Linux and macOS.
    // =============================================================================
    class PosixPlatform final : public IPlatform
    {
        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IPlatform interface
    public:
        Uint32                              GetLogicalCoreCount()   const override;
        double                              GetTimeSeconds()        const override;
        OpaaxString                         GetExecutablePath()     const override;
        OpaaxString                         GetPlatformName()       const override;
        [[nodiscard]] const IFileSystem&    GetFileSystem()         const override { return m_FileSystem; }
        //~End IPlatform interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        StdFileSystem m_FileSystem;
    };
}

#endif // OPAAX_PLATFORM_POSIX
