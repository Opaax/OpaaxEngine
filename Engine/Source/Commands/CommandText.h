#pragma once

#include <nlohmann/json.hpp>

#include "Commands/CommandTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class ICommandRegistry;

    // =============================================================================
    // CommandText — a command typed as a line, read into a command name and its JSON params:
    //   name word word key=value "two words" {json}
    //   Words fill the declared parameters in order; key=value names one. A word that reads as JSON
    //   (5, 0.25, true, [1, 2], {"x": 1}) is that value, unless its parameter is a String; a quoted word
    //   is always text. A lone JSON object after the name is the whole params. Each error names what
    //   is wrong. Pure: the console and console.run use it, scripts send JSON and are not checked.
    // =============================================================================

    /** One word of a line. */
    struct CommandWord
    {
        /** The word, unquoted when it was quoted (JSON escapes read). For key=value, the value. */
        OpaaxString Text;

        /** For key=value, the key; empty for a word that fills the next parameter. */
        OpaaxString Key;

        /** It was in quotes: always text. */
        bool bQuoted = false;
    };

    /** A line read: the command and its params, or why it cannot be. */
    struct CommandTextResult
    {
        bool           bOk = false;
        OpaaxString    Command;
        nlohmann::json Params = nlohmann::json::object();
        OpaaxString    Error;
    };

    /**
     * Splits InLine into words: spaces separate them, "quoted text" (with JSON escapes) and {...} or
     * [...] stay whole. False, with OutError, for a quote or a bracket left open.
     */
    bool SplitCommandText(const OpaaxString& InLine, TDynArray<CommandWord>& OutWords, OpaaxString& OutError);

    /**
     * The params InSpec's command gets from InArgs (the words after its name). A command that declares
     * no parameters takes any key=value, and no plain word.
     */
    CommandTextResult BuildCommandParams(const CommandSpec& InSpec, const TDynArray<CommandWord>& InArgs);

    /** InLine read against the commands of InCommands: an unknown command is an error. */
    CommandTextResult ParseCommandText(const OpaaxString& InLine, const ICommandRegistry& InCommands);

    /** How a command is typed: "time.scale <scale:float>", optional parameters in [ ]. */
    OpaaxString FormatCommandUsage(const CommandSpec& InSpec);

    /** help's list: a row per command, its name and the first sentence of its help, cheats marked. */
    OpaaxString FormatHelpList(const TDynArray<CommandSpec>& InSpecs);

    /** help {command}: its usage, its help, then a row per parameter with its type and choices. */
    OpaaxString FormatCommandHelp(const CommandSpec& InSpec);
}
