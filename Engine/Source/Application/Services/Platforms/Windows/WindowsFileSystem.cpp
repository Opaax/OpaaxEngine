#include "WindowsFileSystem.h"

#ifdef OPAAX_PLATFORM_WINDOWS

#include "WindowsUtf8.h"

#include <filesystem>
#include <system_error>

namespace Opaax
{
    namespace STDFileSyt = std::filesystem;

    namespace
    {
        // Every path enters std::filesystem as UTF-16. Building the path from a wchar_t sequence is
        // the whole fix: the char overload would decode these bytes as ANSI instead.
        STDFileSyt::path ToPath(const OpaaxString& InUtf8)
        {
            return STDFileSyt::path(Windows::Utf8ToWide(InUtf8));
        }
    }

    bool WindowsFileSystem::CreateDirectories(const OpaaxString& InPath) const
    {
        if (InPath.IsEmpty())
        {
            return false;
        }

        std::error_code        lError;
        const STDFileSyt::path lPath = ToPath(InPath);

        STDFileSyt::create_directories(lPath, lError);
        if (lError)
        {
            return false;
        }

        // create_directories reports false when the directory ALREADY existed — a success under this
        // contract — so ask the filesystem what is true now instead of trusting that return.
        const bool bIsDirectory = STDFileSyt::is_directory(lPath, lError);
        return bIsDirectory && !lError;
    }

    bool WindowsFileSystem::IsPathExist(const OpaaxString& InPath) const
    {
        if (InPath.IsEmpty())
        {
            return false;
        }

        std::error_code lError;
        const bool      bExists = STDFileSyt::exists(ToPath(InPath), lError);
        return bExists && !lError;
    }

    bool WindowsFileSystem::ListDirectory(const OpaaxString& InDirAbs, TDynArray<Entry>& OutEntries) const
    {
        if (InDirAbs.IsEmpty())
        {
            return false;
        }

        std::error_code        lError;
        const STDFileSyt::path lDir = ToPath(InDirAbs);

        if (!STDFileSyt::is_directory(lDir, lError) || lError)
        {
            return false;
        }

        // Manual increment with an error_code: the range-for form throws on a mid-walk failure, and a
        // directory CAN change under us between the check above and the walk below.
        STDFileSyt::directory_iterator lIt(lDir, lError);
        if (lError)
        {
            return false;
        }

        const STDFileSyt::directory_iterator lEnd;
        for (; lIt != lEnd && !lError; lIt.increment(lError))
        {
            std::error_code lEntryError;
            const bool      bIsDirectory = lIt->is_directory(lEntryError);

            // One unreadable child does not invalidate the rest of the listing.
            if (lEntryError)
            {
                continue;
            }

            // generic_WSTRING, then one explicit conversion: the narrow generic_string() would encode
            // back through the ANSI code page and hand the caller mojibake it would store as UTF-8.
            OutEntries.push_back(Entry{
                Windows::WideToUtf8(lIt->path().filename().generic_wstring()),
                Windows::WideToUtf8(lIt->path().generic_wstring()),
                bIsDirectory });
        }

        return !lError;
    }
}

#endif // OPAAX_PLATFORM_WINDOWS
