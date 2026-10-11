#pragma once

#include <filesystem>
#include <string>

#include "Core/String/OpaaxString.hpp"
#include "World/Serialization/LevelFile.h"   // LevelData
#include "World/Serialization/MapFile.h"     // MAP_EXTENSION

namespace Opaax::Editor
{
    // =============================================================================
    // NewLevelPlan — what New Level writes, from the paths it was given: a level of one empty map,
    //   named after its file. Paths are asset-relative ("Levels/Arena.opaaxlevel"). Text only: the
    //   command writes the files and opens the level.
    // =============================================================================
    namespace NewLevelPlan
    {
        /** InPath ends with InExtension (".opaaxlevel"), and has a name before it. */
        inline bool HasExtension(const OpaaxString& InPath, const char* InExtension)
        {
            const std::filesystem::path lPath(InPath.CStr());
            return lPath.extension() == InExtension && !lPath.stem().empty();
        }

        /** The map a new level gets when none is named: Maps/<the level's file name>.opaaxmap. */
        inline OpaaxString DefaultMap(const OpaaxString& InLevelPath)
        {
            const std::string lName = std::filesystem::path(InLevelPath.CStr()).stem().generic_string();
            return OpaaxString(("Maps/" + lName + MapFile::MAP_EXTENSION).c_str());
        }

        /** The level file's content: named after its file, InMapPath its only (and persistent) map. */
        inline LevelData Make(const OpaaxString& InLevelPath, const OpaaxString& InMapPath)
        {
            LevelData lData;
            lData.Name = OpaaxString(std::filesystem::path(InLevelPath.CStr()).stem().generic_string().c_str());
            lData.Maps.push_back(InMapPath);
            lData.PersistentMapIndex = 0;
            return lData;
        }
    }
}
