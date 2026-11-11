#pragma once

#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    /**
     * A cheat command answered ok (CommandSpec::Cheat). Published at once on the engine event bus,
     * from whichever source ran it (a script, the console). A game voids a run on it, or reads
     * ICommandRegistry::HaveCheatsRun at the finish line.
     */
    struct CheatUsed
    {
        OpaaxStringID Command;
    };
}
