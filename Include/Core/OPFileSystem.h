#pragma once
#include <filesystem>
#include "OpaaxTypes.hpp"

namespace STDFileSystem = std::filesystem;

/**
 * @class OPFileSystem
 *
 * Utils functions using boost lib.
 *
 * Check, Create Directories.
 * Get Time Stamp as string suitable for directories or not.
 */
class OPFileSystem
{
 //-----------------------------------------------------------------
 // Functions
 //-----------------------------------------------------------------
public:
 /**
  * Create directories for the specified path.
  *
  * @param InPath The path for which directories need to be created.
  * @return true if the directories are successfully created, false otherwise.
  */
 static bool CreateDirectories(const STDFileSystem::path& InPath);
    
 /**
  * Check if the specified path exists.
  *
  * @param InPath The path to check for existence.
  * @return true if the path exists, false otherwise.
  */
 static bool IsPathExist(const STDFileSystem::path& InPath);

 /**
  * Get the parent path of the specified input path.
  *
  * @param InPath The input path for which the parent path is retrieved.
  * @return The parent path of the specified input path.
  */
 static STDFileSystem::path GetPathAtProjectDir(const OString& InPath);

 /**
  * Get the path after creating all necessary directories along the way.
  *
  * @param InPath The input path for which directories need to be created.
  * @return The final path after creating all necessary directories. Empty path if directories cannot be created.
  */
 static STDFileSystem::path GetPathIfNCreate(const OString& InPath);

 /**
  * Get the current timestamp as a string in ISO 8601 extended format.
  *
  * @return The current timestamp as a string in the format "YYYY-MM-DD HH:MM:SS".
  */
 static OString GetTimeStamp();
 
 /**
  * Get the timestamp formatted for directory usage.
  * Replaces spaces with ' ', with '_' in the timestamp obtained from i.e. GetTimeStamp().
  * Replaces spaces with ':', with '-' in the timestamp obtained from i.e. GetTimeStamp().
  * 
  * @return The timestamp string suitable for directory naming.
  */
 static OString GetTimeStampDirectoryFriendly();
};
