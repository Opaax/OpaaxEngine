#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class World;
}

namespace Opaax::Editor
{
    class EditorSelection;
    class EditorUndo;
    struct EditorContext;

    /**
     * Which document's world a step acts on. Resolved when the step is replayed (the level's world is
     * replaced by each Play session, so no pointer is stored).
     */
    enum class EUndoWorld : Uint8
    {
        Active,   // the level (WorldManager's active world)
        Prefab    // the prefab document's own world
    };

    /** The world InScope names right now, or null. */
    World* UndoWorld(const EditorContext& InContext, EUndoWorld InScope);

    /**
     * The selection of that world's document (restoring selects, destroying clears).
     */
    EditorSelection& UndoSelection(const EditorContext& InContext, EUndoWorld InScope);

    /** The undo stack for that scope (each document has its own history). */
    EditorUndo& UndoStack(const EditorContext& InContext, EUndoWorld InScope);
}
