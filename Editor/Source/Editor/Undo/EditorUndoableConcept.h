// EditorUndoableConcept.h
#pragma once

#include "Core/Templates/TypeTraits.h"

namespace Opaax::Editor
{
    /**
     * An undo step: can undo, redo, and give its name. Whoever makes the edit builds it with the data
     * its inverse needs. No base class or registry: a game module's own step is one struct with these
     * three members.
     */
    template<typename T, typename ContextType>
    concept EditorUndoable = requires(T InStep, const T InConstStep, ContextType& InContext)
    {
        { InStep.Undo(InContext) } -> CSameAs<void>;
        { InStep.Redo(InContext) } -> CSameAs<void>;
        { InConstStep.Label() }    -> CConvertibleTo<const char*>;
    };
}
