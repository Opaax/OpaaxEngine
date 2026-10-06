#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    struct FontFamilyData;

    inline constexpr LogCategory LogFontFamilyFile{"FontFamilyFile"};

    // =============================================================================
    // FontFamilyFile — reads and writes .opaaxfont files.
    // =============================================================================
    namespace FontFamilyFile
    {
        /** File extension. */
        inline constexpr const char* FAMILY_EXTENSION = ".opaaxfont";

        /**
         * Writes InData to InAbsPath, replacing any content (dump(4), sorted keys).
         * @param InAbsPath Absolute UTF-8 path
         * @return False if the file could not be written
         */
        bool Save(const OpaaxString& InAbsPath, const FontFamilyData& InData);

        /**
         * Reads InAbsPath into OutData. OutData is untouched on failure.
         * @return False if the file is missing, empty, not JSON, or not an object
         */
        bool Load(const OpaaxString& InAbsPath, FontFamilyData& OutData);

        /** InData as the exact text Save writes (used by the editor's dirty check). */
        OpaaxString Serialize(const FontFamilyData& InData);
    }
}
