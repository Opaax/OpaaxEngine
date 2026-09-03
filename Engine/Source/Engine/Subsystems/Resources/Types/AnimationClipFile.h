#pragma once

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    struct AnimationClipData;

    inline constexpr LogCategory LogAnimationClipFile{"AnimationClipFile"};

    // =============================================================================
    // AnimationClipFile — a clip on DISK: the `.opaaxclip` reader and writer.
    //
    //   SpriteSheetFile's shape, including the part that matters: SAVE AND LOAD ARE ONE UNIT.
    //   They are the two halves of one format contract, and splitting them across files is how a
    //   writer and a reader drift.
    //
    //   Everything goes through Core/IO/FileIO, never <fstream> (**I7**), and WriteAllText creates
    //   missing parents. A clip is a plain aggregate, so the nlohmann macro IS its serializer —
    //   there is no AnimationClipJson layer, for SpriteSheetFile's reason.
    // =============================================================================
    namespace AnimationClipFile
    {
        /** Lowercase, one spelling, matching `.opaaxmap` / `.opaaxlevel` / `.opaaxsheet`. */
        inline constexpr const char* CLIP_EXTENSION = ".opaaxclip";

        /**
         * Write InData to InAbsPath, replacing whatever was there.
         *
         * `dump(4)` with sorted keys — nlohmann's object IS sorted — so a hand-authored file and a
         * written one are byte-identical, the rule **MP6** already holds maps to.
         *
         * @param InAbsPath Absolute UTF-8 path.
         * @return false if the parents could not be created or the file could not be opened. An
         *   EMPTY clip is a success: clearing the steps and saving is a thing an author does.
         */
        OPAAX_API bool Save(const OpaaxString& InAbsPath, const AnimationClipData& InData);

        /**
         * Read InAbsPath into OutData.
         *
         * OutData is left UNTOUCHED on every failure path, so a clip that fails to parse does not
         * half-overwrite the one the caller already had.
         *
         * @return false when the file is missing, empty, not json, or not an object.
         */
        OPAAX_API bool Load(const OpaaxString& InAbsPath, AnimationClipData& OutData);

        /** InData as the exact text Save would write — what the editor's dirty check compares against. */
        OPAAX_API OpaaxString Serialize(const AnimationClipData& InData);
    }
}
