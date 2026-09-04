#pragma once

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    struct SpriteSheetData;

    inline constexpr LogCategory LogSpriteSheetFile{"SpriteSheetFile"};

    // =============================================================================
    // SpriteSheetFile — a sheet on DISK: the `.opaaxsheet` reader and writer.
    //
    //   MapFile's shape one layer over, including the part that matters: SAVE AND LOAD ARE ONE
    //   UNIT. They are the two halves of one format contract, and splitting them across files is
    //   how a writer and a reader drift.
    //
    //   Everything goes through Core/IO/FileIO, never <fstream> (**I7**), and WriteAllText creates
    //   missing parents — which is what every caller means by "save it here".
    //
    //   There is no SpriteSheetJson beside this the way MapJson sits beside MapFile: a sheet is a
    //   plain aggregate, so NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT IS its serializer. A map
    //   needed its own layer because a component payload is opaque json it must not interpret.
    // =============================================================================
    namespace SpriteSheetFile
    {
        /** Lowercase, one spelling, matching `.opaaxmap` / `.opaaxlevel`. */
        inline constexpr const char* SHEET_EXTENSION = ".opaaxsheet";

        /**
         * Write InData to InAbsPath, replacing whatever was there.
         *
         * `dump(4)` with sorted keys — nlohmann's object IS sorted — so a hand-authored file and a
         * written one are byte-identical, the rule **MP6** already holds maps to.
         *
         * @param InAbsPath Absolute UTF-8 path.
         * @return false if the parents could not be created or the file could not be opened. An
         *   EMPTY sheet is a success: clearing the frames and saving is a thing an author does.
         */
        OPAAX_API bool Save(const OpaaxString& InAbsPath, const SpriteSheetData& InData);

        /**
         * Read InAbsPath into OutData.
         *
         * OutData is left UNTOUCHED on every failure path, so a sheet that fails to parse does not
         * half-overwrite the one the caller already had.
         *
         * @return false when the file is missing, empty, not json, or not an object.
         */
        OPAAX_API bool Load(const OpaaxString& InAbsPath, SpriteSheetData& OutData);

        /** InData as the exact text Save would write — what the editor's dirty check compares against. */
        OPAAX_API OpaaxString Serialize(const SpriteSheetData& InData);
    }
}
