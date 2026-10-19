#pragma once

#include "Core/String/OpaaxPathString.h"
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
            return PathString::Extension(InPath) == InExtension && !PathString::Stem(InPath).IsEmpty();
        }

        /** The map a new level gets when none is named: Maps/<the level's file name>.opaaxmap. */
        inline OpaaxString DefaultMap(const OpaaxString& InLevelPath)
        {
            return "Maps/" + PathString::Stem(InLevelPath).ToString() + MapFile::MAP_EXTENSION;
        }

        /** The level file's content: named after its file, InMapPath its only (and persistent) map. */
        inline LevelData Make(const OpaaxString& InLevelPath, const OpaaxString& InMapPath)
        {
            LevelData lData;
            lData.Name = PathString::Stem(InLevelPath).ToString();
            lData.Maps.push_back(InMapPath);
            lData.PersistentMapIndex = 0;
            return lData;
        }
    }
}
