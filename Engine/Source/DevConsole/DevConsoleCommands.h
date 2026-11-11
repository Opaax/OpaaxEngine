#pragma once

#include "Commands/CommandHandle.h"
#include "Commands/CommandTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class DevConsole;
    class ICommandRegistry;

    /**
     * help's list: a row per command, its name and the first sentence of its help, cheats marked. Pure.
     */
    OpaaxString FormatHelpList(const TDynArray<CommandSpec>& InSpecs);

    /** help {command}: its usage, its help, then a row per parameter with its type and choices. Pure. */
    OpaaxString FormatCommandHelp(const CommandSpec& InSpec);

    /**
     * The console's own commands, for the console's lifetime (dev only): help [command], and
     * console.info [rows] (open, the line, the last rows).
     */
    TDynArray<CommandHandle> RegisterDevConsoleCommands(ICommandRegistry& InCommands, DevConsole& InConsole);
}
