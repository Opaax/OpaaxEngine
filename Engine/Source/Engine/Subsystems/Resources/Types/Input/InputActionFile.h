#pragma once

#include "Application/Services/ILogger.h"
#include "Core/EngineAPI.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    struct InputActionData;

    inline constexpr LogCategory LogInputActionFile{"InputActionFile"};

    // =============================================================================
    // InputActionFile — an action on DISK: the `.opaaxaction` reader and writer.
    //
    //   MoverFile's shape exactly, and for its reason: SAVE AND LOAD ARE ONE UNIT.
    // =============================================================================
    namespace InputActionFile
    {
        /** Lowercase, one spelling, matching the rest of the family. */
        inline constexpr const char* INPUT_ACTION_EXTENSION = ".opaaxaction";

        /**
         * Write InData to InAbsPath, replacing whatever was there.
         *
         * @return false if the parents could not be created or the file could not be opened.
         */
        OPAAX_API bool Save(const OpaaxString& InAbsPath, const InputActionData& InData);

        /**
         * Read InAbsPath into OutData. OutData is left UNTOUCHED on every failure path.
         *
         * @return false when the file is missing, empty, not json, or not an object.
         */
        OPAAX_API bool Load(const OpaaxString& InAbsPath, InputActionData& OutData);

        /** InData as the exact text Save would write — what the editor's dirty check compares. */
        OPAAX_API OpaaxString Serialize(const InputActionData& InData);
    }
}
