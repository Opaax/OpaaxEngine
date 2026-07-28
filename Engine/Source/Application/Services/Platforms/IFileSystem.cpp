#include "IFileSystem.h"

#include <filesystem>
#include <iostream>
#include <system_error>

#include "Application/OpaaxApplication.h"
#include "Application/Services/ILogger.h"

namespace Opaax
{
    namespace  STDFileSyt = std::filesystem;

    bool IFileSystem::CreateDirectories(const OpaaxString& InPath) const
    {
        if (InPath.IsEmpty())
        {
            return false;
        }

        std::error_code       lError;
        const STDFileSyt::path lPath(InPath.CStr());

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

    bool IFileSystem::IsPathExist(const OpaaxString& InPath) const
    {
        if (InPath.IsEmpty())
        {
            return false;
        }

        std::error_code lError;
        const bool      bExists = STDFileSyt::exists(STDFileSyt::path(InPath.CStr()), lError);
        return bExists && !lError;
    }

    OpaaxString IFileSystem::GetPathIfNCreate(const OpaaxString& InPath) const
    {
        if (InPath.IsEmpty())
        {
            return {};
        }

        if (CreateDirectories(InPath))
        {
            return InPath;
        }

        // Reachable before the logger is provided (Bootstrap creates directories), hence the fallback.
        ILogger& Logger = OpaaxApplication::GetAppService<ILogger>();

        if (!Logger.IsNull())
        {
            OPAAX_APP_LOG(Error,"IFileSystem cannot create: {}", InPath.CStr());
        }
        else
        {
            std::cout << "IFileSystem cannot create: "<< InPath.CStr() << std::endl;
        }

        return {};
    }

    bool IFileSystem::ListDirectory(const OpaaxString& InDirAbs, TDynArray<Entry>& OutEntries) const
    {
        if (InDirAbs.IsEmpty())
        {
            return false;
        }

        std::error_code        lError;
        const STDFileSyt::path lDir(InDirAbs.CStr());

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

            OutEntries.push_back(Entry{
                OpaaxString(lIt->path().filename().generic_string().c_str()),
                OpaaxString(lIt->path().generic_string().c_str()),
                bIsDirectory });
        }

        return !lError;
    }
}
