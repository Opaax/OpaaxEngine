#include "DevConsole/DevConsoleCommands.h"

#include "Application/Services/ICommandRegistry.h"
#include "Core/String/OpaaxStringJson.h"
#include "DevConsole/DevConsole.h"

namespace Opaax
{
    namespace
    {
        /** Rows console.info answers when it is not told how many. */
        constexpr Uint32 INFO_ROWS = 20;
    }

    TDynArray<CommandHandle> RegisterDevConsoleCommands(ICommandRegistry& InCommands, DevConsole& InConsole)
    {
        TDynArray<CommandHandle> lHandles;

        lHandles.push_back(InCommands.Register(
            CommandSpec("console.info", "The dev console: whether it is open, the line being typed, its caret, "
                                        "and the last {rows} output rows (20), each with its kind.")
                .Param("rows", ECommandParamType::Int, "how many of the last rows").Optional()
                .DevOnly(),
            [&InConsole](const nlohmann::json& InParams)
            {
                return CommandResult::Ok(InConsole.Info(InParams.value("rows", INFO_ROWS)));
            }));

        return lHandles;
    }
}
