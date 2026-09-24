#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Tag/OpaaxTag.h"
#include "Editor/Commands/EditorCommandRegistry.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // IEditorCommandParams — a command payload stored with its STATIC TYPE intact.
    //
    //   EditorCommandRegistry::Execute<TParams> is a template and its typeid gate is what makes a
    //   wrong payload a logged refusal rather than a reinterpret_cast, so a caller that holds a
    //   payload for later cannot erase it to void*. Dispatch() is the box calling that template with
    //   the type it was built from.
    //
    //   In Commands/ rather than Menus/: a menu entry is the first holder, a key->tag binding will
    //   be the second, and both need this exact box.
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
