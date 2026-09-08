#pragma once

#include "Application/Services/ILogger.h"
#include "Core/EngineAPI.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    struct InputMappingContextData;

    inline constexpr LogCategory LogInputMappingContextFile{"InputMapFile"};

    // =============================================================================
    // InputMappingContextFile — a mapping context on DISK: the `.opaaxinputmap` reader and
    //   writer. MoverFile's shape exactly, and for its reason: SAVE AND LOAD ARE ONE UNIT.
    // =============================================================================
    namespace InputMappingContextFile
    {
        /** Lowercase, one spelling, matching the rest of the family. */
        inline constexpr const char* INPUT_MAP_EXTENSION = ".opaaxinputmap";

        /**
         * Write InData to InAbsPath, replacing whatever was there.
         *
         * @return false if the parents could not be created or the file could not be opened. An
         *   EMPTY context is a success — it is what a freshly created one is.
         */
        OPAAX_API bool Save(const OpaaxString& InAbsPath, const InputMappingContextData& InData);

        /**
         * Read InAbsPath into OutData. OutData is left UNTOUCHED on every failure path.
         *
         * @return false when the file is missing, empty, not json, or not an object.
         */
        OPAAX_API bool Load(const OpaaxString& InAbsPath, InputMappingContextData& OutData);

        /** InData as the exact text Save would write — what the editor's dirty check compares. */
        OPAAX_API OpaaxString Serialize(const InputMappingContextData& InData);
    }
}
