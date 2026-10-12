#include "Platform/StdFileSystem.h"

#include "Core/IO/FileSystem.h"

namespace Opaax
{
    bool StdFileSystem::CreateDirectories(const OpaaxString& InPath) const
    {
        return FileSystem::CreateDirectories(InPath);
    }

    bool StdFileSystem::IsPathExist(const OpaaxString& InPath) const
    {
        return FileSystem::Exists(InPath);
    }

    bool StdFileSystem::ListDirectory(const OpaaxString& InDirAbs, TDynArray<Entry>& OutEntries) const
    {
        return FileSystem::ListDirectory(InDirAbs, OutEntries);
    }
}
