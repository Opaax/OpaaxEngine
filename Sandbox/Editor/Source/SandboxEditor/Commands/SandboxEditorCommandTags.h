#pragma once

#include "Core/Tag/OpaaxTag.h"

namespace SandboxEditor::Tags
{
    // The GAME's own command tags, in the game's own namespace — the editor never learns them.
    // `inline` on purpose: a namespace-scope `const` has INTERNAL linkage, so every including TU
    // would build — and intern — its own copy (I14's ctor interns).

    inline const Opaax::OpaaxTag SANDBOX_COMMAND_VALIDATE = Opaax::OpaaxTag("Sandbox.Command.Validate");
}
