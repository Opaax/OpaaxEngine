#pragma once

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    struct AnimationLibraryData;

    inline constexpr LogCategory LogAnimationLibraryFile{"AnimationLibraryFile"};

    // =============================================================================
    // AnimationLibraryFile — a library on DISK: the `.opaaxanim` reader and writer.
    //
    //   AnimationClipFile's shape exactly, and for its reason: SAVE AND LOAD ARE ONE UNIT.
    // =============================================================================
    namespace AnimationLibraryFile
    {
        /** Lowercase, one spelling, matching `.opaaxsheet` / `.opaaxclip`. */
        inline constexpr const char* LIBRARY_EXTENSION = ".opaaxanim";

        /**
         * Write InData to InAbsPath, replacing whatever was there.
         *
         * @return false if the parents could not be created or the file could not be opened. An
         *   EMPTY library is a success — it is what a freshly created one is.
         */
        OPAAX_API bool Save(const OpaaxString& InAbsPath, const AnimationLibraryData& InData);

        /**
         * Read InAbsPath into OutData. OutData is left UNTOUCHED on every failure path.
         *
         * @return false when the file is missing, empty, not json, or not an object.
         */
        OPAAX_API bool Load(const OpaaxString& InAbsPath, AnimationLibraryData& OutData);

        /** InData as the exact text Save would write — what the editor's dirty check compares against. */
        OPAAX_API OpaaxString Serialize(const AnimationLibraryData& InData);
    }
}
