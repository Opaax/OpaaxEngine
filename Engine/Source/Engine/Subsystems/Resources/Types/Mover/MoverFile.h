#pragma once

#include "Application/Services/ILogger.h"
#include "Core/EngineAPI.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    struct MoverData;

    inline constexpr LogCategory LogMoverFile{"MoverFile"};

    // =============================================================================
    // MoverFile — a mover on DISK: the `.opaaxmover` reader and writer.
    //
    //   AnimationLibraryFile's shape exactly, and for its reason: SAVE AND LOAD ARE ONE UNIT.
    // =============================================================================
    namespace MoverFile
    {
        /** Lowercase, one spelling, matching the rest of the family. */
        inline constexpr const char* MOVER_EXTENSION = ".opaaxmover";

        /**
         * Write InData to InAbsPath, replacing whatever was there.
         *
         * @return false if the parents could not be created or the file could not be opened. An
         *   EMPTY mover is a success — it is what a freshly created one is.
         */
        OPAAX_API bool Save(const OpaaxString& InAbsPath, const MoverData& InData);

        /**
         * Read InAbsPath into OutData. OutData is left UNTOUCHED on every failure path.
         *
         * @return false when the file is missing, empty, not json, or not an object.
         */
        OPAAX_API bool Load(const OpaaxString& InAbsPath, MoverData& OutData);

        /** InData as the exact text Save would write — what the editor's dirty check compares against. */
        OPAAX_API OpaaxString Serialize(const MoverData& InData);
    }
}
