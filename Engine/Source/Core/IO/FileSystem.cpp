#include "Core/IO/FileSystem.h"

#include "Core/String/OpaaxUtf8.h"

#include <filesystem>
#include <system_error>

namespace Opaax::FileSystem
{
    namespace fs = std::filesystem;

    // =============================================================================
    // Questions
    // =============================================================================
    bool Exists(const OpaaxString& InPath)
    {
        if (InPath.IsEmpty()) { return false; }

        std::error_code lError;
        const bool      bExists = fs::exists(Utf8::ToFsPath(InPath), lError);
        return bExists && !lError;
    }

    bool IsFile(const OpaaxString& InPath)
    {
        if (InPath.IsEmpty()) { return false; }

        std::error_code lError;
        const bool      bIsFile = fs::is_regular_file(Utf8::ToFsPath(InPath), lError);
        return bIsFile && !lError;
    }

    bool IsDirectory(const OpaaxString& InPath)
    {
        if (InPath.IsEmpty()) { return false; }

        std::error_code lError;
        const bool      bIsDirectory = fs::is_directory(Utf8::ToFsPath(InPath), lError);
        return bIsDirectory && !lError;
    }

    bool ListDirectory(const OpaaxString& InDir, TDynArray<Entry>& OutEntries)
    {
        if (!IsDirectory(InDir)) { return false; }

        // Manual increment with an error_code: the range-for throws on a mid-walk failure.
        std::error_code         lError;
        fs::directory_iterator lIt(Utf8::ToFsPath(InDir), lError);
        if (lError) { return false; }

        const fs::directory_iterator lEnd{};
        for (; lIt != lEnd && !lError; lIt.increment(lError))
        {
            std::error_code lEntryError;
            const bool      bIsDirectory = lIt->is_directory(lEntryError);

            // One unreadable entry does not stop the listing.
            if (lEntryError) { continue; }

            OutEntries.push_back({ Utf8::FromFsPath(lIt->path().filename()), Utf8::FromFsPath(lIt->path()), bIsDirectory });
        }

        return !lError;
    }

    // =============================================================================
    // Changes
    // =============================================================================
    bool CreateDirectories(const OpaaxString& InPath)
    {
        if (InPath.IsEmpty()) { return false; }

        std::error_code lError;
        fs::create_directories(Utf8::ToFsPath(InPath), lError);

        // create_directories returns false when it already existed: ask what is there now.
        return !lError && IsDirectory(InPath);
    }

    bool Remove(const OpaaxString& InPath)
    {
        if (InPath.IsEmpty()) { return false; }

        std::error_code lError;
        const bool      bRemoved = fs::remove(Utf8::ToFsPath(InPath), lError);
        return bRemoved && !lError;
    }

    bool RemoveAll(const OpaaxString& InPath)
    {
        if (InPath.IsEmpty()) { return false; }

        std::error_code lError;
        fs::remove_all(Utf8::ToFsPath(InPath), lError);
        return !lError;
    }

    bool Rename(const OpaaxString& InFrom, const OpaaxString& InTo)
    {
        if (InFrom.IsEmpty() || InTo.IsEmpty()) { return false; }

        std::error_code lError;
        fs::rename(Utf8::ToFsPath(InFrom), Utf8::ToFsPath(InTo), lError);
        return !lError;
    }

    bool Copy(const OpaaxString& InFrom, const OpaaxString& InTo, const bool bInOverwrite)
    {
        if (InFrom.IsEmpty() || InTo.IsEmpty()) { return false; }

        std::error_code lError;
        const bool      bCopied = fs::copy_file(Utf8::ToFsPath(InFrom), Utf8::ToFsPath(InTo),
                                                bInOverwrite ? fs::copy_options::overwrite_existing
                                                             : fs::copy_options::none,
                                                lError);
        return bCopied && !lError;
    }

    // =============================================================================
    // Paths
    // =============================================================================
    OpaaxString WorkingDirectory()
    {
        std::error_code lError;
        const fs::path  lPath = fs::current_path(lError);
        return lError ? OpaaxString() : Utf8::FromFsPath(lPath);
    }

    OpaaxString TempDirectory()
    {
        std::error_code lError;
        const fs::path  lPath = fs::temp_directory_path(lError);
        return lError ? OpaaxString() : Utf8::FromFsPath(lPath);
    }

    OpaaxString MakeAbsolute(const OpaaxString& InPath)
    {
        std::error_code lError;
        const fs::path  lPath = fs::absolute(Utf8::ToFsPath(InPath), lError);
        return lError ? OpaaxString() : Utf8::FromFsPath(lPath);
    }

    OpaaxString MakeCanonical(const OpaaxString& InPath)
    {
        std::error_code lError;
        const fs::path  lPath = fs::weakly_canonical(Utf8::ToFsPath(InPath), lError);
        return lError ? OpaaxString() : Utf8::FromFsPath(lPath);
    }
}
