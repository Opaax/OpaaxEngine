#pragma once

#include "Application/Services/ILogger.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class UIWidget;
    class UIWidgetRegistry;

    inline constexpr LogCategory LogUICanvasFile{"UICanvasFile"};

    // =============================================================================
    // UICanvasFile — a widget tree on DISK: the `.opaaxui` reader and writer.
    //
    //   SAVE AND LOAD ARE ONE UNIT, `FontFamilyFile`'s stated rule — they are the two halves of one
    //   format contract, and splitting them across files is how a writer and a reader drift. The
    //   text form is public too: the editor's dirty check compares against it and its undo step
    //   carries a whole tree as one (**UI12**).
    //
    //   A node names its TYPE and carries its own fields; the registry turns the name into an empty
    //   widget and the widget reads itself (SaveFields/LoadFields). An UNKNOWN type is SKIPPED with
    //   one warning — a file from a build that knows more widget types still opens.
    // =============================================================================
    namespace UICanvasFile
    {
        /** Lowercase, one spelling, matching `.opaaxsheet` / `.opaaxfont` / `.opaaxprefab`. */
        inline constexpr const char* UI_EXTENSION = ".opaaxui";

        /** Bumped when the shape changes. A file carrying a newer one is refused, not guessed at. */
        inline constexpr Uint32 UI_FORMAT_VERSION = 1;

        /** What a canvas is, as a file: its reference height and its root's children. */
        struct UICanvasDoc
        {
            float                ReferenceHeight = 1080.f;
            TUniquePtr<UIWidget> Root;              // a UIPanel; its children are the authored tree
        };

        /**
         * InDoc as text — `dump(4)`, keys sorted by nlohmann's object, NO trailing newline, so a
         * hand-inspected file and a written one are byte-identical (**MP6**'s rule for maps).
         */
        OPAAX_API OpaaxString Serialize(const UICanvasDoc& InDoc);

        /**
         * The same text from a root that something else OWNS — the editor's live canvas, whose tree
         * must not be moved into a doc just to be written.
         */
        OPAAX_API OpaaxString Serialize(const UIWidget& InRoot, float InReferenceHeight);

        /**
         * Parse InText into OutDoc, building widgets through InRegistry.
         *
         * OutDoc is left UNTOUCHED on every failure path, so a file that fails to parse does not
         * half-overwrite the tree the caller already had.
         *
         * @return false on malformed json or a version this build cannot read.
         */
        OPAAX_API bool Deserialize(const OpaaxString& InText, const UIWidgetRegistry& InRegistry, UICanvasDoc& OutDoc);

        /** Write InDoc to InAbsPath, replacing whatever was there. */
        OPAAX_API bool Save(const OpaaxString& InAbsPath, const UICanvasDoc& InDoc);

        /** Read InAbsPath into OutDoc. Deserialize's guarantees, plus "the file could not be read". */
        OPAAX_API bool Load(const OpaaxString& InAbsPath, const UIWidgetRegistry& InRegistry, UICanvasDoc& OutDoc);

        /** How many widgets InRoot's subtree holds, itself included. The log's number. */
        OPAAX_API Uint64 CountWidgets(const UIWidget& InRoot);
    }
}
