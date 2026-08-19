#pragma once

#include "Editor/Commands/EditorCommandConcept.h"   // NoParams

namespace Opaax::Editor
{
    struct EditorContext;
}

namespace SandboxEditor
{
    /**
     * A check the GAME defines and the editor knows nothing about: every authored entity should
     * carry the game's own HealthComponent, and the tag count beside it exercises the hierarchical
     * match (I14).
     *
     * A COMMAND rather than a menu closure, which is what makes it reachable by tag from anywhere —
     * a key binding, another panel, a second menu entry — instead of only from the one entry that
     * used to carry its body. It satisfies EditorCommand<EditorContext> exactly as the editor's own
     * verbs do: same concept, same registry, no privileged path (D10).
     */
    struct ValidateSandboxCommand
    {
        using Params = Opaax::Editor::NoParams;

        void Execute(Opaax::Editor::EditorContext& InContext, const Params&);
    };
}
