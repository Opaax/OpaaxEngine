#pragma once

#include "Core/Log/Logger.h"
#include "Core/EngineAPI.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    struct InputMappingContextData;

    inline constexpr LogCategory LogInputMappingContextFile{"InputMapFile"};

    // =============================================================================
    // InputMappingContextFile — reads and writes .opaaxinputmap files.
    // =============================================================================
    namespace InputMappingContextFile
    {
        /** File extension. */
        inline constexpr const char* INPUT_MAP_EXTENSION = ".opaaxinputmap";

        /**
         * Writes InData to InAbsPath, replacing any content (dump(4), sorted keys).
         * @param InAbsPath Absolute UTF-8 path
         * @return False if the file could not be written
         */
        bool Save(const OpaaxString& InAbsPath, const InputMappingContextData& InData);

        /**
         * Reads InAbsPath into OutData. OutData is untouched on failure.
         * @return False if the file is missing, empty, not JSON, or not an object
         */
        bool Load(const OpaaxString& InAbsPath, InputMappingContextData& OutData);

        /** InData as the exact text Save writes (used by the editor's dirty check). */
        OpaaxString Serialize(const InputMappingContextData& InData);
    }
}
