#pragma once

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"

#include "World/Serialization/MapData.h"

namespace Opaax
{
    inline constexpr LogCategory LogMapFile{"MapFile"};

    // =============================================================================
    // MapFile — a Map on DISK: the `.opaaxmap` reader and writer.
    //
    //   The stack, top to bottom:  World  <-MapSerializer/MapFactory->  MapData
    //                                     <-MapJson->                   json
    //                                     <-MapFile->                   a file
    //   Each layer knows only its neighbours, which is why the snapshot core needed no
    //   change to gain persistence.
    //
    //   Everything goes through Core/IO/FileIO, never <fstream> directly (**I7**): a stream
    //   opened from OpaaxString::CStr() resolves a DIFFERENT FILE than the caller named, with
    //   no error anywhere. FileIO::WriteAllText also creates missing parent directories, which
    //   is what every caller means by "save the map here".
    //
    //   SAVE AND LOAD ARE ONE UNIT, deliberately. They are the two halves of one format
    //   contract, and splitting them across files is how a writer and a reader drift. That
    //   MapResource is the thing that *loads* a map in the running engine does not make the
    //   writer belong somewhere else — MapResource::Load calls straight through to here.
    // =============================================================================
    namespace MapFile
    {
        /** Lowercase, matching the shipped `.opaaxproj` (**WM4**). The ONE spelling. */
        inline constexpr const char* MAP_EXTENSION = ".opaaxmap";

        /**
         * "…/Maps/Decor.opaaxmap" -> `MapId("Decor")` — what a file at this path would be called.
         *
         * The LAST resort for identity when loading (**MP10**), and the FIRST answer when creating:
         * a map being authored has no entities to claim it, so its own name is all there is. Public
         * because both sides need it and a second copy of a naming rule is how two of them drift.
         *
         * @return An invalid id when the path has no stem at all.
         */
        OPAAX_API MapId StemId(const OpaaxString& InAbsPath);

        /**
         * Write InData to InAbsPath, replacing whatever was there.
         *
         * Missing parent directories are CREATED (FileIO::WriteAllText's contract) — "Save As"
         * into a folder the user just named must not fail because the folder is not there yet.
         *
         * @param InAbsPath Absolute UTF-8 path.
         * @return false if the parents could not be created or the file could not be opened.
         *   Saving an EMPTY map is a success, not a failure: clearing a world and saving it is
         *   a thing an author does on purpose.
         */
        OPAAX_API bool Save(const OpaaxString& InAbsPath, const MapData& InData);

        /**
         * Read InAbsPath into OutData.
         *
         * OutData IS LEFT UNTOUCHED ON FAILURE, all the way down — MapJson::FromJson holds the
         * same contract — so a failed load cannot half-replace the map the caller already had
         * and get written back over the original by the next save.
         *
         * @param InAbsPath Absolute UTF-8 path.
         * @return false when the file is missing/unreadable, or its contents are not a map this
         *   build can read (malformed json, or a NEWER format version — see MapJson).
         */
        OPAAX_API bool Load(const OpaaxString& InAbsPath, MapData& OutData);
    }
}
