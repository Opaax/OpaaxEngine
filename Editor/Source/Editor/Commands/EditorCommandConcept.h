// EditorCommandConcept.h
#pragma once

#include <concepts>

namespace Opaax::Editor
{
    /**
     * The payload of a command that takes no arguments.
     *
     * One shared type rather than an empty struct per command: the registry's typeid gate exists to
     * catch a wrong PAYLOAD, and two commands that both take nothing have no payload to confuse.
     */
    struct NoParams {};

    template<typename T, typename ContextType>
    concept EditorCommand =
        requires(T Command, ContextType& Context, const typename T::Params& Params)
    {
        { Command.Execute(Context, Params) } -> std::same_as<void>;
    };
}
