// EditorUndoableConcept.h
#pragma once

#include <concepts>

namespace Opaax::Editor
{
    /**
     * An undo step: it can put the editor back, forward again, and say what it is called.
     *
     * THE STACK KNOWS NOTHING ABOUT WHAT CHANGED. Whoever made the edit is the one that knows, so
     * it builds the object and gives it exactly the data its own inverse needs — a rename carries
     * two strings, a drag carries two transforms, a delete carries the entities. No base class and
     * no registry: a game module's own step is one struct with these three members.
     */
    template<typename T, typename ContextType>
    concept EditorUndoable = requires(T InStep, const T InConstStep, ContextType& InContext)
    {
        { InStep.Undo(InContext) } -> std::same_as<void>;
        { InStep.Redo(InContext) } -> std::same_as<void>;
        { InConstStep.Label() }    -> std::convertible_to<const char*>;
    };
}
