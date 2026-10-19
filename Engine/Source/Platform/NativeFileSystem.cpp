#include "Platform/NativeFileSystem.h"

#include "Core/IO/FileSystem.h"

namespace Opaax
{
    bool NativeFileSystem::CreateDirectories(const OpaaxString& InPath) const
    {
        return FileSystem::CreateDirectories(InPath);
    }

    bool NativeFileSystem::IsPathExist(const OpaaxString& InPath) const
    {
        return FileSystem::Exists(InPath);
    }

    bool NativeFileSystem::ListDirectory(const OpaaxString& InDirAbs, TDynArray<Entry>& OutEntries) const
    {
        return FileSystem::ListDirectory(InDirAbs, OutEntries);
    }
}
