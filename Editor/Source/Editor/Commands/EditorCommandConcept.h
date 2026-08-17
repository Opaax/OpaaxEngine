// EditorCommandConcept.h
#pragma once

#include <concepts>

namespace Opaax::Editor
{
    template<typename T, typename ContextType>
    concept EditorCommand =
        requires(T Command, ContextType& Context, const typename T::Params& Params)
    {
        { Command.Execute(Context, Params) } -> std::same_as<void>;
    };

    //template<typename T, typename ContextType>
    //concept UndoableEditorCommand =
    //    EditorCommand<T, ContextType> &&
    //    requires(T Command, ContextType& Context)
    //{
    //    { Command.Undo(Context) } -> std::same_as<void>;
    //};
}
