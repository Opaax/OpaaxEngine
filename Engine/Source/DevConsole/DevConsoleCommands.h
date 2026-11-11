#pragma once

#include "Commands/CommandHandle.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class DevConsole;
    class ICommandRegistry;

    /**
     * The console's own commands, for the console's lifetime (dev only): console.info [rows] (open,
     * the line, the last rows).
     */
    TDynArray<CommandHandle> RegisterDevConsoleCommands(ICommandRegistry& InCommands, DevConsole& InConsole);
}
