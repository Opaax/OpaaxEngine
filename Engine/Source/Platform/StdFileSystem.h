#pragma once

#include "Platform/IFileSystem.h"

namespace Opaax
{
    // =============================================================================
    // StdFileSystem — IFileSystem on std::filesystem, for every platform. Paths go through
    //   Utf8::ToFsPath / FromFsPath, so UTF-8 names work on Windows too.
    // =============================================================================
    class StdFileSystem final : public IFileSystem
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
