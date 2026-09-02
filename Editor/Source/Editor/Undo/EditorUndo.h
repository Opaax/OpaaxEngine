// EditorUndo.h
#pragma once

#include "Core/OpaaxTypes.h"
#include "Editor/Undo/IEditorUndoable.h"

namespace Opaax::Editor
{
    struct EditorContext;

    /**
     * How many steps a session keeps, dropping the oldest past it.
     *
     * It bounds the memory the history holds, which is whatever the steps carry — a rename is two
     * strings, a delete is its entities.
     */
    inline constexpr Uint64 MAX_UNDO_STEPS = 100;

    // =============================================================================
    // EditorUndo — THE STACK, and nothing else.
    //
    //   Record, Undo, Redo. It holds undoable objects, replays them, and names them for the Edit
    //   menu. It knows nothing about worlds, entities, components, the selection or the UI, and it
    //   includes no engine header at all: whoever made the edit is the one that knows what changed,
    //   so it is the one that builds the step (**UN1**).
    //
    //   Owned by EditorService and reached through EditorContext — the EditorSelection/EditorGizmo
    //   shape: the writers are the verbs and the panels, the readers are the Edit menu and the
    //   shortcuts, and no panel owns either.
    //
    //   IT DOES NOT DECIDE WHETHER AN EDIT IS LEGAL. Undo and Redo are gated on MapOps::CanEdit by
    //   the two commands that drive them, which is where every other editor policy already lives.
    // =============================================================================
    class EditorUndo
    {
        // =============================================================================
        // Record
        // =============================================================================
    public:
        /**
         * Keep InStep as the newest step, dropping everything that was redoable.
         *
         * A no-op while a step is being replayed, so an undo cannot record itself as the next step.
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
        /** Put the newest step back, and move it to the redo stack. */
        void Undo(EditorContext& InContext);

        /** Put the newest undone step forward again, and move it back to the undo stack. */
        void Redo(EditorContext& InContext);

        bool CanUndo() const noexcept { return !m_Undo.empty(); }
        bool CanRedo() const noexcept { return !m_Redo.empty(); }

        /** What the next step is called, or "" — the Edit menu's "Undo Move". */
        const char* UndoLabel() const noexcept;
        const char* RedoLabel() const noexcept;

        // =============================================================================
        // Lifetime
        // =============================================================================
    public:
        /**
         * Drop the history.
         *
         * Called by EditorService when the edited world is destroyed, and by the one structural
         * level verb that unmounts a map — a step naming an entity of an unmounted map would
         * recreate it into a world no Save can write it from (**WM2**).
         */
        void Clear() noexcept;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** The one place a step reaches the stack: redo cleared, cap applied, one line logged. */
        void Push(IEditorUndoable&& InStep);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<IEditorUndoable> m_Undo;
        TDynArray<IEditorUndoable> m_Redo;

        // True while a step is being replayed. PRIVATE: Record is what asks, so nothing outside
        // has to remember to.
        bool m_bApplying = false;
    };
}
