#include "Core/OPFileSystem.h"
#include "Boost/OPBoostTypes.h"
#include "Core/OPLogMacro.h"


bool OPFileSystem::CreateDirectories(const STDFileSystem::path& InPath)
{
    return STDFileSystem::create_directories(InPath);
}

bool OPFileSystem::IsPathExist(const STDFileSystem::path& InPath)
{
    return STDFileSystem::exists(InPath);
}

STDFileSystem::path OPFileSystem::GetPathAtProjectDir(const OString& InPath)
{
    return STDFileSystem::path(InPath).parent_path();
}

STDFileSystem::path OPFileSystem::GetPathIfNCreate(const OString& InPath)
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

OString OPFileSystem::GetTimeStamp()
{
    // Get the current time using Boost
    BSTTime lNow = BSTSecondClock::local_time();

    // Format the time as a string
    OString lTimestamp = BSTPosixTime::to_iso_extended_string(lNow);

    // Replace 'T' with a space for better readability
    std::ranges::replace(lTimestamp, 'T', ' ');

    return lTimestamp;
}

OString OPFileSystem::GetTimeStampDirectoryFriendly()
{
    OString lTimestamp = GetTimeStamp();

    // Replace ' ' with a '_' for directory
    std::ranges::replace(lTimestamp, ' ', '_');
    // Replace ':' with a '-' for directory
    std::ranges::replace(lTimestamp, ':', '-');

    return lTimestamp;
}
