#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Tag/OpaaxTag.h"
#include "Editor/Commands/EditorCommandRegistry.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // IEditorCommandParams — a command payload stored with its static type, so it can be dispatched
    //   later through EditorCommandRegistry::Execute<TParams> (which checks the type). Dispatch()
    //   calls that template with the type the box was built from.
    // =============================================================================
    struct IEditorCommandParams
    {
        virtual ~IEditorCommandParams() = default;

        virtual void Dispatch(const EditorCommandRegistry& InCommands, const OpaaxTag& InTag,
                              EditorContext& InContext) const = 0;
    };

    template<typename T>
    struct EditorCommandParamsBox final : IEditorCommandParams
    {
        explicit EditorCommandParamsBox(T InValue) : Value(Move(InValue)) {}

        void Dispatch(const EditorCommandRegistry& InCommands, const OpaaxTag& InTag,
                      EditorContext& InContext) const override
        {
            InCommands.Execute(InTag, InContext, Value);
        }

        T Value;
    };
}
