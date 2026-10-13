#pragma once

#include "Platform/IFileSystem.h"

namespace Opaax
{
    // =============================================================================
    // NativeFileSystem — IFileSystem on the core FileSystem, for every platform. UTF-8 names work
    //   on Windows too.
    // =============================================================================
    class NativeFileSystem final : public IFileSystem
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
