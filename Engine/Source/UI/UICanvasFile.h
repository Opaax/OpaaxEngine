#pragma once

#include "Core/Log/Logger.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class UIWidget;
    class UIWidgetRegistry;

    inline constexpr LogCategory LogUICanvasFile{"UICanvasFile"};

    // =============================================================================
    // UICanvasFile — reads and writes .opaaxui files. A node names its type and holds its fields;
    //   the registry creates the widget and the widget reads itself. Unknown types are skipped
    //   (with one warning).
    // =============================================================================
    namespace UICanvasFile
    {
        /** File extension. */
        inline constexpr const char* UI_EXTENSION = ".opaaxui";

        /** Format version. A newer version is refused. */
        inline constexpr Uint32 UI_FORMAT_VERSION = 1;

        /** A canvas file: reference height and the root's children. */
        struct UICanvasDoc
        {
            float                ReferenceHeight = 1080.f;
            TUniquePtr<UIWidget> Root;              // a UIPanel; its children are the authored tree
        };

        /**
         * InDoc as text: dump(4), sorted keys, no trailing newline.
         */
        OpaaxString Serialize(const UICanvasDoc& InDoc);

        /**
         * The same text from a root owned by someone else (the editor's live canvas).
         */
        OpaaxString Serialize(const UIWidget& InRoot, float InReferenceHeight);

        /**
         * Parses InText into OutDoc, creating widgets through InRegistry. OutDoc is untouched on failure.
         * @return False on malformed JSON or an unsupported version
         */
        bool Deserialize(const OpaaxString& InText, const UIWidgetRegistry& InRegistry, UICanvasDoc& OutDoc);

        /** Writes InDoc to InAbsPath, replacing any content. */
        bool Save(const OpaaxString& InAbsPath, const UICanvasDoc& InDoc);

        /** Reads InAbsPath into OutDoc (same guarantees as Deserialize). */
        bool Load(const OpaaxString& InAbsPath, const UIWidgetRegistry& InRegistry, UICanvasDoc& OutDoc);

        /** Number of widgets in InRoot's subtree, itself included. */
        Uint64 CountWidgets(const UIWidget& InRoot);

        // =============================================================================
        // One node (the clipboard unit, so paste works across documents).
        // =============================================================================

        /** InWidget and its subtree as text. */
        OpaaxString SerializeNode(const UIWidget& InWidget);

        /** A new subtree from InText, or null if it is not a node. */
        TUniquePtr<UIWidget> DeserializeNode(const OpaaxString& InText, const UIWidgetRegistry& InRegistry);

        /** A deep copy through the file format. */
        TUniquePtr<UIWidget> CloneWidget(const UIWidget& InWidget, const UIWidgetRegistry& InRegistry);
    }
}
