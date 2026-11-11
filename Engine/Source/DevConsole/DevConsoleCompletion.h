#pragma once

#include "Commands/CommandTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    /** What Tab did to a line. */
    struct DevConsoleCompletion
    {
        /** The line with its last word completed (as it was when nothing fits). */
        OpaaxString Line;

        /** Every name or choice that fits, when several do: the console lists them. */
        TDynArray<OpaaxString> Matches;
    };

    /**
     * Completes the last word of InLine (the text before the caret), as Tab does: the first word among
     * the commands' names, a later one among the choices of the parameter it fills (key=value: that
     * key's choices). One match is written whole with a space after it; several, as far as they agree,
     * and listed. Case does not matter for matching; the match's own case is written. Pure.
     */
    DevConsoleCompletion CompleteCommandText(const OpaaxString& InLine, const TDynArray<CommandSpec>& InSpecs);
}
