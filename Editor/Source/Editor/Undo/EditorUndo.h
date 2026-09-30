// EditorUndo.h
#pragma once

#include "Core/OpaaxTypes.h"
#include "Editor/Undo/IEditorUndoable.h"

namespace Opaax::Editor
{
    struct EditorContext;

    /**
     * How many steps a session keeps; the oldest are dropped past it. Bounds the history's memory.
     */
    inline constexpr Uint64 MAX_UNDO_STEPS = 100;

    // =============================================================================
    // EditorUndo — the undo stack: Record, Undo, Redo, and step names for the Edit menu.
    //   Knows nothing about worlds, entities or UI: whoever makes an edit builds its step.
    //   Owned by EditorService, reached through EditorContext. Whether an edit is allowed is decided
    //   by the Undo/Redo commands (MapOps::CanEdit), not here.
    // =============================================================================
    class EditorUndo
    {
        // =============================================================================
        // Record
        // =============================================================================
    public:
        /**
         * Keeps InStep as the newest step, dropping everything redoable.
         * Does nothing while a step is being replayed (an undo cannot record itself).
         */
        template<EditorUndoable<EditorContext> T>
        void Record(T InStep)
        {
            if (m_bApplying) { return; }

            Push(IEditorUndoable(Move(InStep)));
        }

        // =============================================================================
        // Playback
        // =============================================================================
    public:
        /** Undoes the newest step and moves it to the redo stack. */
        void Undo(EditorContext& InContext);

        /** Redoes the newest undone step and moves it back to the undo stack. */
        void Redo(EditorContext& InContext);

        bool CanUndo() const noexcept { return !m_Undo.empty(); }
        bool CanRedo() const noexcept { return !m_Redo.empty(); }

        /** The next step's name, or "" (the Edit menu's "Undo Move"). */
        const char* UndoLabel() const noexcept;
        const char* RedoLabel() const noexcept;

        // =============================================================================
        // Lifetime
        // =============================================================================
    public:
        /**
         * Clears the history. Called when the edited world is destroyed, and when a map is unloaded
         * (a step naming one of its entities could not be saved anymore).
         */
        void Clear() noexcept;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** Pushes a step: clears redo, applies the cap, logs. */
        void Push(IEditorUndoable&& InStep);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<IEditorUndoable> m_Undo;
        TDynArray<IEditorUndoable> m_Redo;

        // True while a step is being replayed. Checked by Record.
        bool m_bApplying = false;
    };
}
