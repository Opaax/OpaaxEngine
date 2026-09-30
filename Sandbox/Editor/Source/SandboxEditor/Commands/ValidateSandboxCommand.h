#pragma once

#include "Editor/Commands/EditorCommandConcept.h"   // NoParams

namespace Opaax::Editor
{
    struct EditorContext;
}

namespace SandboxEditor
{
    /**
     * A check defined by the game: every authored entity should have a HealthComponent. Also counts
     * tags with a hierarchical match. A command, so it can be reached by tag (menu, key binding...).
     */
    struct ValidateSandboxCommand
    {
        using Params = Opaax::Editor::NoParams;

        void Execute(Opaax::Editor::EditorContext& InContext, const Params&);
    };
}
