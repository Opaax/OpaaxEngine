#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    struct AnimationClipData;

    inline constexpr LogCategory LogAnimationClipFile{"AnimationClipFile"};

    // =============================================================================
    // AnimationClipFile — reads and writes .opaaxclip files.
    // =============================================================================
    namespace AnimationClipFile
    {
        /** File extension. */
        inline constexpr const char* CLIP_EXTENSION = ".opaaxclip";

        /**
         * Writes InData to InAbsPath, replacing any content (dump(4), sorted keys).
         * @param InAbsPath Absolute UTF-8 path
         * @return False if the file could not be written
         */
        bool Save(const OpaaxString& InAbsPath, const AnimationClipData& InData);

        /**
         * Reads InAbsPath into OutData. OutData is untouched on failure.
         * @return False if the file is missing, empty, not JSON, or not an object
         */
        bool Load(const OpaaxString& InAbsPath, AnimationClipData& OutData);

        /** InData as the exact text Save writes (used by the editor's dirty check). */
        OpaaxString Serialize(const AnimationClipData& InData);
    }
}
