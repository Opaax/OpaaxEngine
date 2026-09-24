#pragma once

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"

#include "World/Prefab/PrefabData.h"

namespace Opaax
{
    inline constexpr LogCategory LogPrefabFile{"PrefabFile"};

    // =============================================================================
    // PrefabFile — a Prefab on DISK: the `.opaaxprefab` reader and writer.
    //
    //   MapFile's shape, one document over, and for its reasons: everything goes through
    //   Core/IO/FileIO rather than <fstream> (**I7**), and SAVE AND LOAD ARE ONE UNIT because
    //   they are the two halves of one format contract — splitting them across files is how a
    //   writer and a reader drift.
    // =============================================================================
    namespace PrefabFile
    {
        /** Lowercase, matching every other Opaax extension (**WM4**). The ONE spelling. */
        inline constexpr const char* PREFAB_EXTENSION = ".opaaxprefab";

        /**
         * Write InData to InAbsPath, replacing whatever was there.
         *
         * Missing parent directories are CREATED (FileIO::WriteAllText's contract) — saving into a
         * folder the author just named must not fail because the folder is not there yet.
         *
         * @return false if the file could not be written. Saving an EMPTY prefab is a SUCCESS: it
         *   is the state a freshly created one is in.
         */
        OPAAX_API bool Save(const OpaaxString& InAbsPath, const PrefabData& InData);

        /**
         * Read InAbsPath into OutData.
         *
         * OutData IS LEFT UNTOUCHED ON FAILURE, all the way down — PrefabJson::FromJson holds the
         * same contract — so a failed load cannot half-replace the prefab the caller already had
         * and get written back over the original by the next save.
         *
         * @return false when the file is missing/unreadable, or its contents are not a prefab this
         *   build can read (malformed json, or a NEWER format version).
         */
        OPAAX_API bool Load(const OpaaxString& InAbsPath, PrefabData& OutData);
    }
}
