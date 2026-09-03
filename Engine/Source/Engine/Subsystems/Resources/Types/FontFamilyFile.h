#pragma once

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    struct FontFamilyData;

    inline constexpr LogCategory LogFontFamilyFile{"FontFamilyFile"};

    // =============================================================================
    // FontFamilyFile — a family on DISK: the `.opaaxfont` reader and writer.
    //
    //   SpriteSheetFile's shape exactly, including the part that matters: SAVE AND LOAD ARE ONE
    //   UNIT. They are the two halves of one format contract, and splitting them across files is how
    //   a writer and a reader drift.
    //
    //   Save has no caller in the engine YET — the shipped `Roboto.opaaxfont` was generated once,
    //   because asset CREATION is reserved for the editor's factory (**AN8**) and a one-off menu
    //   entry would pre-empt it. It is here because a format's two halves are written together or
    //   they diverge, and because Serialize is what a dirty check compares against the moment a
    //   family editor exists.
    // =============================================================================
    namespace FontFamilyFile
    {
        /** Lowercase, one spelling, matching `.opaaxsheet` / `.opaaxclip` / `.opaaxanim`. */
        inline constexpr const char* FAMILY_EXTENSION = ".opaaxfont";

        /**
         * Write InData to InAbsPath, replacing whatever was there.
         *
         * `dump(4)` with sorted keys — nlohmann's object IS sorted — so a hand-authored file and a
         * written one are byte-identical, the rule **MP6** already holds maps to.
         *
         * @return false if the parents could not be created or the file could not be opened.
         */
        OPAAX_API bool Save(const OpaaxString& InAbsPath, const FontFamilyData& InData);

        /**
         * Read InAbsPath into OutData.
         *
         * OutData is left UNTOUCHED on every failure path, so a family that fails to parse does not
         * half-overwrite the one the caller already had.
         *
         * @return false when the file is missing, empty, not json, or not an object.
         */
        OPAAX_API bool Load(const OpaaxString& InAbsPath, FontFamilyData& OutData);

        /** InData as the exact text Save would write. */
        OPAAX_API OpaaxString Serialize(const FontFamilyData& InData);
    }
}
