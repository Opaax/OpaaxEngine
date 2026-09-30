#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    struct AnimationLibraryData;

    inline constexpr LogCategory LogAnimationLibraryFile{"AnimationLibraryFile"};

    // =============================================================================
    // AnimationLibraryFile — reads and writes .opaaxanim files.
    // =============================================================================
    namespace AnimationLibraryFile
    {
        /** File extension. */
        inline constexpr const char* LIBRARY_EXTENSION = ".opaaxanim";

        /**
         * Writes InData to InAbsPath, replacing any content (dump(4), sorted keys).
         * @param InAbsPath Absolute UTF-8 path
         * @return False if the file could not be written
         */
        OPAAX_API bool Save(const OpaaxString& InAbsPath, const AnimationLibraryData& InData);

        /**
         * Reads InAbsPath into OutData. OutData is untouched on failure.
         * @return False if the file is missing, empty, not JSON, or not an object
         */
        OPAAX_API bool Load(const OpaaxString& InAbsPath, AnimationLibraryData& OutData);

        /** InData as the exact text Save writes (used by the editor's dirty check). */
        OPAAX_API OpaaxString Serialize(const AnimationLibraryData& InData);
    }
}
