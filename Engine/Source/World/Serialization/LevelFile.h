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

        /**
         * Index into Maps of the ALWAYS-MOUNTED map (**WM1a**) — the one holding what every other
         * map of this level composes on top of, so the player and the lights are authored once and
         * never dragged into another map again.
         *
         * An INDEX rather than a second path: "the persistent map is one of this level's maps" then
         * holds structurally, and LevelFile::Load is the only place that has to check it.
         * 0 when the manifest names none, and when it names one that is not in Maps (**MP3**).
         */
        Uint64 PersistentMapIndex = 0;

        bool   IsEmpty()  const noexcept { return Maps.empty(); }
        Uint64 MapCount() const noexcept { return static_cast<Uint64>(Maps.size()); }

        /** The always-mounted map. Empty when the level names no maps at all. */
        const OpaaxString& PersistentMap() const noexcept
        {
            static const OpaaxString EMPTY;
            return IsEmpty() ? EMPTY : Maps[PersistentMapIndex];
        }
    };

    // =============================================================================
    // LevelFile — the `.opaaxlevel` reader AND writer, in one unit (**MP4**): the two are halves
    //   of one format contract, and splitting them across files is how a writer and a reader
    //   drift. There is no `LevelJson` beside this the way `MapJson` sits beside `MapFile` —
    //   that layer exists because a MapData carries INTERNED ids that must be written as strings
    //   (**MP1**), and a LevelData is a name and some paths.
    // =============================================================================
    namespace LevelFile
    {
        /** Lowercase, matching `.opaaxmap` and the shipped `.opaaxproj` (**WM4**). */
        inline constexpr const char* LEVEL_EXTENSION = ".opaaxlevel";

        /** Same policy as a map: bumped only for a change a v1 reader would MISREAD. */
        inline constexpr Uint32 LEVEL_FORMAT_VERSION = 1;

        inline constexpr const char* KEY_VERSION        = "version";
        inline constexpr const char* KEY_NAME           = "name";
        inline constexpr const char* KEY_MAPS           = "maps";
        inline constexpr const char* KEY_PERSISTENT_MAP = "persistentMap";

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

        /**
         * InData as the text this build writes — indented, since a manifest is meant to be read
         * and diffed.
         *
         * Separate from Save because the editor's dirty check compares against it without writing
         * anything, exactly as `MapJson::Serialize` serves `EditorMapDocument` (**MP5**).
         */
        OPAAX_API OpaaxString Serialize(const LevelData& InData);

        /** Serialize + write. @return false when the file could not be written. */
        OPAAX_API bool Save(const OpaaxString& InAbsPath, const LevelData& InData);
    }
}
