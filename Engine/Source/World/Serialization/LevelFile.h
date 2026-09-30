#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Log/Logger.h"

namespace Opaax
{
    inline constexpr LogCategory LogLevelFile{"LevelFile"};

    // =============================================================================
    // LevelData — a level: its name and its maps, in order.
    // Map paths are asset-relative ("Maps/Main.opaaxmap").
    // =============================================================================
    struct LevelData
    {
        /**
         * The level's name, also the name of the world opened from it.
         * Set by Load: the "name" key, or the file name.
         */
        OpaaxString Name;

        TDynArray<OpaaxString> Maps;

        /**
         * Index in Maps of the persistent (always mounted) map. 0 when none is named, or the named
         * one is not in Maps.
         */
        Uint64 PersistentMapIndex = 0;

        bool   IsEmpty()  const noexcept { return Maps.empty(); }
        Uint64 MapCount() const noexcept { return static_cast<Uint64>(Maps.size()); }

        /** The persistent map. Empty when the level has no maps. */
        const OpaaxString& PersistentMap() const noexcept
        {
            static const OpaaxString EMPTY;
            return IsEmpty() ? EMPTY : Maps[PersistentMapIndex];
        }
    };

    // =============================================================================
    // LevelFile — reads and writes .opaaxlevel files.
    // =============================================================================
    namespace LevelFile
    {
        /** File extension. */
        inline constexpr const char* LEVEL_EXTENSION = ".opaaxlevel";

        /** Bumped only for a change an older reader would misread. */
        inline constexpr Uint32 LEVEL_FORMAT_VERSION = 1;

        inline constexpr const char* KEY_VERSION        = "version";
        inline constexpr const char* KEY_NAME           = "name";
        inline constexpr const char* KEY_MAPS           = "maps";
        inline constexpr const char* KEY_PERSISTENT_MAP = "persistentMap";

        /**
         * Reads InAbsPath into OutData. Never throws; OutData is untouched on failure.
         * @return False if the file is missing, unreadable, not valid JSON, or a newer version
         */
        OPAAX_API bool Load(const OpaaxString& InAbsPath, LevelData& OutData);

        /**
         * InData as the text Save writes (used by the editor's dirty check).
         */
        OPAAX_API OpaaxString Serialize(const LevelData& InData);

        /** Serializes and writes. @return False if the file could not be written. */
        OPAAX_API bool Save(const OpaaxString& InAbsPath, const LevelData& InData);
    }
}
