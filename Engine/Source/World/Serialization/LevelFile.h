#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Application/Services/ILogger.h"

namespace Opaax
{
    inline constexpr LogCategory LogLevelFile{"LevelFile"};

    // =============================================================================
    // LevelData — a Level as DATA: what it is called, and which Maps compose it, in order.
    // Map paths are ASSET-RELATIVE ("Maps/Main.opaaxmap"), resolved through IPaths::AssetToAbsolute.
    // =============================================================================
    struct LevelData
    {
        /**
         * The level's authored name, and therefore the NAME OF THE WORLD opened from it.
         *
         * Always populated by a successful Load: the `name` key when present, the file's stem
         * otherwise. The world's name is the level's own data — nothing upstream mines it out
         * of a path.
         */
        OpaaxString Name;

        TDynArray<OpaaxString> Maps;

        bool   IsEmpty()  const noexcept { return Maps.empty(); }
        Uint64 MapCount() const noexcept { return static_cast<Uint64>(Maps.size()); }
    };

    // =============================================================================
    // LevelFile — the `.opaaxlevel` reader. READ ONLY: nothing authors a level yet.
    // =============================================================================
    namespace LevelFile
    {
        /** Lowercase, matching `.opaaxmap` and the shipped `.opaaxproj` (**WM4**). */
        inline constexpr const char* LEVEL_EXTENSION = ".opaaxlevel";

        /** Same policy as a map: bumped only for a change a v1 reader would MISREAD. */
        inline constexpr Uint32 LEVEL_FORMAT_VERSION = 1;

        inline constexpr const char* KEY_VERSION = "version";
        inline constexpr const char* KEY_NAME    = "name";
        inline constexpr const char* KEY_MAPS    = "maps";

        /**
         * Read InAbsPath into OutData. Tolerant and total — never throws.
         *
         * OutData is LEFT UNTOUCHED on failure, the same contract MapFile::Load holds, so a
         * level that fails to read cannot half-replace one already loaded.
         *
         * @return false when the file is missing/unreadable, is not valid json, or declares a
         *   version NEWER than this build reads.
         */
        OPAAX_API bool Load(const OpaaxString& InAbsPath, LevelData& OutData);
    }
}
