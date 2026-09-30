// EditorCommandConcept.h
#pragma once

#include <concepts>

namespace Opaax::Editor
{
    /**
     * The payload of a command that takes no arguments.
     */
    struct NoParams {};

    template<typename T, typename ContextType>
    concept EditorCommand =
        requires(T Command, ContextType& Context, const typename T::Params& Params)
    {
        { Command.Execute(Context, Params) } -> std::same_as<void>;
    };
}
