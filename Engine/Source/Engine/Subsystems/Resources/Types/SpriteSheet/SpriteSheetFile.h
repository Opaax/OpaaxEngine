#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    struct SpriteSheetData;

    inline constexpr LogCategory LogSpriteSheetFile{"SpriteSheetFile"};

    // =============================================================================
    // SpriteSheetFile — reads and writes .opaaxsheet files.
    // =============================================================================
    namespace SpriteSheetFile
    {
        /** File extension. */
        inline constexpr const char* SHEET_EXTENSION = ".opaaxsheet";

        /**
         * Writes InData to InAbsPath, replacing any content (dump(4), sorted keys).
         * @param InAbsPath Absolute UTF-8 path
         * @return False if the file could not be written
         */
        bool Save(const OpaaxString& InAbsPath, const SpriteSheetData& InData);

        /**
         * Reads InAbsPath into OutData. OutData is untouched on failure.
         * @return False if the file is missing, empty, not JSON, or not an object
         */
        bool Load(const OpaaxString& InAbsPath, SpriteSheetData& OutData);

        /** InData as the exact text Save writes (used by the editor's dirty check). */
        OpaaxString Serialize(const SpriteSheetData& InData);
    }
}
