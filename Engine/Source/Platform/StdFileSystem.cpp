#include "Platform/StdFileSystem.h"

#include "Core/String/OpaaxUtf8.h"

#include <filesystem>
#include <system_error>

namespace Opaax
{
    namespace StdFs = std::filesystem;

    bool StdFileSystem::CreateDirectories(const OpaaxString& InPath) const
    {
        if (InPath.IsEmpty())
        {
            return false;
        }

        std::error_code   lError;
        const StdFs::path lPath = Utf8::ToFsPath(InPath);

        StdFs::create_directories(lPath, lError);
        if (lError)
        {
            return false;
        }

        // create_directories returns false if it already existed: check that it exists now.
        const bool bIsDirectory = StdFs::is_directory(lPath, lError);
        return bIsDirectory && !lError;
    }

    bool StdFileSystem::IsPathExist(const OpaaxString& InPath) const
    {
        if (InPath.IsEmpty())
        {
            return false;
        }

        std::error_code lError;
        const bool      bExists = StdFs::exists(Utf8::ToFsPath(InPath), lError);
        return bExists && !lError;
    }

    bool StdFileSystem::ListDirectory(const OpaaxString& InDirAbs, TDynArray<Entry>& OutEntries) const
    {
        if (InDirAbs.IsEmpty())
        {
            return false;
        }

        std::error_code   lError;
        const StdFs::path lDir = Utf8::ToFsPath(InDirAbs);

        if (!StdFs::is_directory(lDir, lError) || lError)
        {
            return false;
        }

        // Manual increment with an error_code: the range-for throws on a mid-walk failure.
        StdFs::directory_iterator lIt(lDir, lError);
        if (lError)
        {
            return false;
        }

        const StdFs::directory_iterator lEnd{};
        for (; lIt != lEnd && !lError; lIt.increment(lError))
        {
            std::error_code lEntryError;
            const bool      bIsDirectory = lIt->is_directory(lEntryError);

            // One unreadable entry does not stop the listing.
            if (lEntryError)
            {
                continue;
            }

            OutEntries.emplace_back(Utf8::FromFsPath(lIt->path().filename()),
                                    Utf8::FromFsPath(lIt->path()),
                                    bIsDirectory);
        }

        return !lError;
    }
}
