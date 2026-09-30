#pragma once

#include "Platform/IFileSystem.h"
#include "Core/EngineAPI.h"

#ifdef OPAAX_PLATFORM_WINDOWS

namespace Opaax
{
    // =============================================================================
    // WindowsFileSystem — IFileSystem for Windows (held by WindowsPlatform).
    //   Converts UTF-8 to UTF-16 on the way in and back on the way out; the work uses
    //   std::filesystem on wide paths.
    // =============================================================================
    class OPAAX_API WindowsFileSystem final : public IFileSystem
    {
        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IFileSystem interface
    public:
        bool CreateDirectories(const OpaaxString& InPath)   const override;
        bool IsPathExist(const OpaaxString& InPath)         const override;
        bool ListDirectory(const OpaaxString& InDirAbs, TDynArray<Entry>& OutEntries) const override;
        //~End IFileSystem interface
    };
}

#endif // OPAAX_PLATFORM_WINDOWS
