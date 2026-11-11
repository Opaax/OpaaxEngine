#include "DevConsole/DevConsoleCommands.h"

#include "Application/Services/ICommandRegistry.h"
#include "Commands/CommandText.h"
#include "Core/Maths/Maths.h"
#include "Core/String/OpaaxStringJson.h"
#include "DevConsole/DevConsole.h"

namespace Opaax
{
    namespace
    {
        /** Rows console.info answers when it is not told how many. */
        constexpr Uint32 INFO_ROWS = 20;

        /** InHelp up to the end of its first sentence. */
        OpaaxString FirstSentence(const OpaaxString& InHelp)
        {
            for (Uint32 lAt = 0; lAt + 1 < InHelp.GetLength(); ++lAt)
            {
                if (InHelp[lAt] == '.' && InHelp[lAt + 1] == ' ')
                {
                    return InHelp.SubString(0, lAt + 1);
                }
            }
            return InHelp;
        }

        /** InText with spaces after it up to InWidth characters (at least one). */
        OpaaxString PadRight(const OpaaxString& InText, const Uint32 InWidth)
        {
            OpaaxString lText = InText;
            do
            {
                lText += ' ';
            }
            while (lText.GetLength() < InWidth);
            return lText;
        }
    }

    OpaaxString FormatHelpList(const TDynArray<CommandSpec>& InSpecs)
    {
        Uint32 lWidth = 0;
        for (const CommandSpec& lSpec : InSpecs)
        {
            lWidth = Maths::Max(lWidth, lSpec.GetName().GetLength());
        }

        OpaaxString lText;
        for (const CommandSpec& lSpec : InSpecs)
        {
            lText += PadRight(lSpec.GetName(), lWidth + 2);
            lText += FirstSentence(lSpec.GetHelp());
            if (lSpec.IsCheat())
            {
                lText += " [cheat]";
            }
            lText += '\n';
        }
        lText += "help <command> describes one; Tab completes a name.";
        return lText;
    }

    OpaaxString FormatCommandHelp(const CommandSpec& InSpec)
    {
        OpaaxString lText = FormatCommandUsage(InSpec);
        if (InSpec.IsCheat())
        {
            lText += "  [cheat]";
        }
        lText += '\n';
        lText += InSpec.GetHelp();

        for (const CommandParam& lParam : InSpec.GetParams())
        {
            lText += "\n  ";
            lText += lParam.Name;
            lText += OpaaxString::Format(" ({}{})", ToString(lParam.Type), lParam.bOptional ? ", optional" : "");
            if (!lParam.Help.IsEmpty())
            {
                lText += ": ";
                lText += lParam.Help;
            }
            if (!lParam.Choices.empty())
            {
                lText += lParam.Type == ECommandParamType::Any ? ". For example " : ". One of ";
                for (Uint64 lIndex = 0; lIndex < lParam.Choices.size(); ++lIndex)
                {
                    lText += lIndex == 0 ? "" : ", ";
                    lText += lParam.Choices[lIndex];
                }
            }
        }
        return lText;
    }

    TDynArray<CommandHandle> RegisterDevConsoleCommands(ICommandRegistry& InCommands, DevConsole& InConsole)
    {
        TDynArray<CommandHandle> lHandles;

        lHandles.push_back(InCommands.Register(
            CommandSpec("help", "Lists the commands, or describes one {command}: its parameters and their choices.")
                .Param("command", ECommandParamType::String, "the command to describe").Optional().NamesCommand()
                .DevOnly(),
            [&InCommands](const nlohmann::json& InParams)
            {
                const OpaaxString lName = InParams.value("command", OpaaxString());
                if (lName.IsEmpty())
                {
                    return CommandResult::Ok(nlohmann::json(FormatHelpList(InCommands.GetSpecs())));
                }

                const TSharedPtr<const RegisteredCommand> lCommand = InCommands.Find(lName);
                if (lCommand == nullptr)
                {
                    return CommandResult::Fail("unknown command '" + lName + "' (help lists them)");
                }
                return CommandResult::Ok(nlohmann::json(FormatCommandHelp(lCommand->Spec)));
            }));

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
