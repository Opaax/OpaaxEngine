#include "Opaax/Filesystem/OpaaxFileSystem.h"

#include "Opaax/Boost/OPBoostTypes.h"
#include "Opaax/Log/OPLogMacro.h"


bool OpaaxFileSystem::CreateDirectories(const STDFileSystem::path& InPath)
{
    return STDFileSystem::create_directories(InPath);
}

bool OpaaxFileSystem::IsPathExist(const STDFileSystem::path& InPath)
{
    return STDFileSystem::exists(InPath);
}

STDFileSystem::path OpaaxFileSystem::GetPathAtProjectDir(const OSTDString& InPath)
{
    return STDFileSystem::path(InPath).parent_path();
}

STDFileSystem::path OpaaxFileSystem::GetPathIfNCreate(const OSTDString& InPath)
{
    STDFileSystem::path lPath = GetPathAtProjectDir(InPath);

    if(!IsPathExist(lPath))
    {
        try
        {
            // Create all directories in the path
            CreateDirectories(lPath);
            OPAAX_ERROR("[%1%] Created directory: %2%: ", %"OPFileSystem" %lPath)
            return lPath;
        }
        catch ([[maybe_unused]] const STDFileSystem::filesystem_error& lE)
        {
            OPAAX_ERROR("[%1%] Can't create directory: %2%", %"OPFileSystem" %lPath)
            return {};
        }
    }
    
    return lPath;
}

OSTDString OpaaxFileSystem::GetTimeStamp()
{
    // Get the current time using Boost
    BSTTime lNow = BSTSecondClock::local_time();

    // Format the time as a string
    OSTDString lTimestamp = BSTPosixTime::to_iso_extended_string(lNow);

    // Replace 'T' with a space for better readability
    std::ranges::replace(lTimestamp, 'T', ' ');

    return lTimestamp;
}

OSTDString OpaaxFileSystem::GetTimeStampDirectoryFriendly()
{
    OSTDString lTimestamp = GetTimeStamp();

    // Replace ' ' with a '_' for directory
    std::ranges::replace(lTimestamp, ' ', '_');
    // Replace ':' with a '-' for directory
    std::ranges::replace(lTimestamp, ':', '-');

    return lTimestamp;
}
