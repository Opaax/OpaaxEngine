#pragma once

#include "Core/Log/Logger.h"
#include "Core/EngineAPI.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    struct InputActionData;

    inline constexpr LogCategory LogInputActionFile{"InputActionFile"};

    // =============================================================================
    // InputActionFile — reads and writes .opaaxaction files.
    // =============================================================================
    namespace InputActionFile
    {
        /** File extension. */
        inline constexpr const char* INPUT_ACTION_EXTENSION = ".opaaxaction";

        /**
         * Writes InData to InAbsPath, replacing any content (dump(4), sorted keys).
         * @param InAbsPath Absolute UTF-8 path
         * @return False if the file could not be written
         */
        bool Save(const OpaaxString& InAbsPath, const InputActionData& InData);

        /**
         * Reads InAbsPath into OutData. OutData is untouched on failure.
         * @return False if the file is missing, empty, not JSON, or not an object
         */
        bool Load(const OpaaxString& InAbsPath, InputActionData& OutData);

        /** InData as the exact text Save writes (used by the editor's dirty check). */
        OpaaxString Serialize(const InputActionData& InData);
    }
}
