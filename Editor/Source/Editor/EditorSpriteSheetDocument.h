#pragma once

#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/Types/SpriteSheet/SpriteSheetData.h"

namespace Opaax::Editor
{
    inline constexpr LogCategory LogEditorSpriteSheetDocument{"EditorSpriteSheetDocument"};

    // =============================================================================
    // EditorSpriteSheetDocument — WHICH `.opaaxsheet` is open, its live data, and whether that data
    //   still matches what was last written.
    //
    //   IT HOLDS THE DATA, unlike EditorMapDocument which is a cursor into a world the engine owns.
    //   Nothing else owns a sheet being edited: the resource in the ResourceManager is what the
    //   RENDERER reads, and editing that copy would change what the game draws mid-edit and lose the
    //   changes on the next reload. So the editor loads its own, and a Save is what publishes it.
    //
    //   THE DIRTY MARKER IS DERIVED, never a flag: IsDirty compares the serialized text against the
    //   baseline captured at Open/Save. A bool would have to be set by every mutation, and the one
    //   that forgets is a `*` that lies (the trap EditorLevelDocument's own baseline exists for).
    //
    //   Reached through EditorContext, like every other editor system: the WRITER is a resource-type
    //   activate closure, the READERS are the panel, the Save command and the undo steps.
    // =============================================================================
    class EditorSpriteSheetDocument
    {
        // =============================================================================
        // Ctor
        // =============================================================================
    public:
        EditorSpriteSheetDocument() = default;

        EditorSpriteSheetDocument(const EditorSpriteSheetDocument&)            = delete;
        EditorSpriteSheetDocument& operator=(const EditorSpriteSheetDocument&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Read InAbsPath and make it the open sheet.
         *
         * A file that fails to load leaves the previous one open and answers false — opening a
         * broken sheet must not silently close the one being worked on.
         */
        bool Open(const OpaaxString& InAbsPath);

        /** Nothing open. Discards unsaved edits — the caller is what asks first. */
        void Close();

        /** Take the current data as the new baseline. Called after a successful Save. */
        void MarkSaved();

        // =============================================================================
        // Get
        // =============================================================================
    public:
        bool               IsOpen()  const noexcept { return !m_AbsPath.IsEmpty(); }
        const OpaaxString& AbsPath() const noexcept { return m_AbsPath; }

        /** Just the file name, for the panel title — "Hero.opaaxsheet". Empty when none is open. */
        OpaaxString FileName() const;

        const SpriteSheetData& GetData() const noexcept { return m_Data; }

        /** The editable copy. Every mutation goes through a verb that also records an undo step. */
        SpriteSheetData& GetMutableData() noexcept { return m_Data; }

        /** Whether the data differs from what was last written. Recomputed, never cached. */
        bool IsDirty() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString     m_AbsPath;
        SpriteSheetData m_Data;

        /** The serialized text as of the last Open/Save — what IsDirty compares against. */
        OpaaxString     m_Baseline;
    };
}
