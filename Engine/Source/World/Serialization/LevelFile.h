#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Application/Services/ILogger.h"

namespace Opaax
{
    inline constexpr LogCategory LogLevelFile{"LevelFile"};

    // =============================================================================
    // LevelData — a Level as DATA: which Maps compose it, in order.
    //
    //   WM1's middle noun. A World is the runtime container and the ECS boundary, a Map is pure
    //   entity data and therefore the serialization unit, and a Level COMPOSES maps — it owns no
    //   entities of its own and never has.
    //
    //   THE MANIFEST STAYS DATA (**WM4**). There is deliberately no `Level` runtime object, no
    //   `LevelManager` subsystem and no streaming in M5: a list of map refs is all any caller
    //   needs today, and the machinery that turns it into a stream policy has nothing asking for
    //   it yet. What lands now is the level -> map INDIRECTION, which is the part streaming will
    //   be built on top of rather than instead of.
    //
    //   Map paths are ASSET-RELATIVE ("Maps/Main.opaaxmap"), resolved through
    //   IPaths::AssetToAbsolute — the same way the project file expresses `startupLevel`, so a
    //   path means the same thing wherever it is written.
    // =============================================================================
    struct LevelData
    {
        TDynArray<OpaaxString> Maps;

        bool   IsEmpty()  const noexcept { return Maps.empty(); }
        Uint64 MapCount() const noexcept { return static_cast<Uint64>(Maps.size()); }
    };

    // =============================================================================
    // LevelFile — the `.opaaxlevel` reader.
    //
    //   READ ONLY, on purpose: nothing in M5 authors a level. The map editor writes `.opaaxmap`
    //   (MapFile::Save); a level is composed by hand until something can actually edit one, and
    //   a Save with no caller is surface to keep working for free.
    // =============================================================================
    namespace LevelFile
    {
        /** Lowercase, matching `.opaaxmap` and the shipped `.opaaxproj` (**WM4**). */
        inline constexpr const char* LEVEL_EXTENSION = ".opaaxlevel";

        /** Same policy as a map: bumped only for a change a v1 reader would MISREAD. */
        inline constexpr Uint32 LEVEL_FORMAT_VERSION = 1;

        inline constexpr const char* KEY_VERSION = "version";
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
