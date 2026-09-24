#pragma once

#include "Application/Services/ILogger.h"
#include "Core/EngineAPI.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    struct MoveModeData;

    inline constexpr LogCategory LogMoveModeFile{"MoveModeFile"};

    // =============================================================================
    // MoveModeFile — a tuning on DISK: the `.opaaxmovemode` reader and writer.
    //
    //   AnimationClipFile's shape exactly, and for its reason: SAVE AND LOAD ARE ONE UNIT.
    // =============================================================================
    namespace MoveModeFile
    {
        /** Lowercase, one spelling, matching `.opaaxsheet` / `.opaaxclip` / `.opaaxanim`. */
        inline constexpr const char* MOVE_MODE_EXTENSION = ".opaaxmovemode";

        /**
         * Write InData to InAbsPath, replacing whatever was there.
         *
         * @return false if the parents could not be created or the file could not be opened.
         */
        OPAAX_API bool Save(const OpaaxString& InAbsPath, const MoveModeData& InData);

        /**
         * Read InAbsPath into OutData. OutData is left UNTOUCHED on every failure path.
         *
         * @return false when the file is missing, empty, not json, or not an object.
         */
        OPAAX_API bool Load(const OpaaxString& InAbsPath, MoveModeData& OutData);

        /** InData as the exact text Save would write — what the editor's dirty check compares against. */
        OPAAX_API OpaaxString Serialize(const MoveModeData& InData);
    }
}
