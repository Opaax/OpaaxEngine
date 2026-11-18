#pragma once

#include "Core/Log/Logger.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Object/ObjectPtr.h"

namespace Opaax
{
    class ObjectClassRegistry;
    class UIWidget;

    inline constexpr LogCategory LogUICanvasFile{"UICanvasFile"};

    // =============================================================================
    // UICanvasFile — reads and writes .opaaxui files. A node names its widget class ("Type") and holds
    //   that class's properties, then its "Modifiers" (each a "Type" and its properties, optional) and
    //   its "Children"; the class registry makes the objects and each class reads its fields. Unknown
    //   types are skipped (with one warning); a value of the wrong type refuses the whole read. An
    //   older version's nodes are upgraded when read (UpgradeCanvasNode), and the current one is written.
    // =============================================================================
    namespace UICanvasFile
    {
        /** File extension. */
        inline constexpr const char* UI_EXTENSION = ".opaaxui";

        /**
         * Format version. A newer version is refused. 2: an image's Color, Texture, Sheet, Frame, Border
         * and BorderScale are in its Brush (Color as Tint); a button's colours and frames are its Brushes.
         */
        inline constexpr Uint32 UI_FORMAT_VERSION = 2;

        /** A canvas file: reference height and the root's children. */
        struct UICanvasDoc
        {
            float                      ReferenceHeight = 1080.f;
            TStrongObjectPtr<UIWidget> Root;   // a UIPanel; its children are the authored tree
        };

        /**
         * A new canvas designed at InReferenceHeight: an empty root panel that fills the screen it is mounted
         * on, so its children anchor to the screen's edges and corners.
         */
        UICanvasDoc MakeEmpty(float InReferenceHeight);

        /**
         * InDoc as text: dump(4), sorted keys, no trailing newline.
         */
        OpaaxString Serialize(const UICanvasDoc& InDoc);

        /**
         * The same text from a root owned by someone else (the editor's live canvas).
         */
        OpaaxString Serialize(const UIWidget& InRoot, float InReferenceHeight);

        /**
         * Parses InText into OutDoc, making widgets of the classes InClasses holds. OutDoc is untouched on
         * failure.
         * @return False on malformed JSON, an unsupported version or a value of the wrong type
         */
        bool Deserialize(const OpaaxString& InText, const ObjectClassRegistry& InClasses, UICanvasDoc& OutDoc);

        /** Writes InDoc to InAbsPath, replacing any content. */
        bool Save(const OpaaxString& InAbsPath, const UICanvasDoc& InDoc);

        /** Reads InAbsPath into OutDoc (same guarantees as Deserialize). */
        bool Load(const OpaaxString& InAbsPath, const ObjectClassRegistry& InClasses, UICanvasDoc& OutDoc);

        /** Number of widgets in InRoot's subtree, itself included. */
        Uint64 CountWidgets(const UIWidget& InRoot);

        // =============================================================================
        // One node (the clipboard unit, so paste works across documents).
        // =============================================================================

        /** InWidget and its subtree as text, with the format version beside its type. */
        OpaaxString SerializeNode(const UIWidget& InWidget);

        /**
         * A new subtree from InText, or null if it is not a node or is of a newer version. A node with
         * no version is from version 1 and is upgraded.
         */
        TStrongObjectPtr<UIWidget> DeserializeNode(const OpaaxString& InText, const ObjectClassRegistry& InClasses);

        /** A deep copy through the file format. */
        TStrongObjectPtr<UIWidget> CloneWidget(const UIWidget& InWidget, const ObjectClassRegistry& InClasses);
    }
}
