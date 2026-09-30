#pragma once

#include "Core/Tag/OpaaxTag.h"

namespace SandboxEditor::Tags
{
    // The game's own command tags, in the game's namespace (the editor never knows them).
    // inline: a namespace-scope const would give every TU its own copy.

    inline const Opaax::OpaaxTag SANDBOX_COMMAND_VALIDATE = Opaax::OpaaxTag("Sandbox.Command.Validate");
}
