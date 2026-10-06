#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"

#include "World/Prefab/PrefabData.h"

namespace Opaax
{
    inline constexpr LogCategory LogPrefabFile{"PrefabFile"};

    // =============================================================================
    // PrefabFile — reads and writes .opaaxprefab files.
    // =============================================================================
    namespace PrefabFile
    {
        /** File extension. */
        inline constexpr const char* PREFAB_EXTENSION = ".opaaxprefab";

        /**
         * Writes InData to InAbsPath, replacing any content. Creates missing directories.
         * @return False if the file could not be written
         */
        bool Save(const OpaaxString& InAbsPath, const PrefabData& InData);

        /**
         * Reads InAbsPath into OutData. OutData is untouched on failure.
         * @return False if the file is missing, unreadable, malformed, or a newer format version
         */
        bool Load(const OpaaxString& InAbsPath, PrefabData& OutData);
    }
}
