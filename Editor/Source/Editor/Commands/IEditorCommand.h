// IEditorCommand.h
#pragma once

#include <memory>
#include <utility>

#include "Core/OpaaxTypes.h"
#include "Editor/EditorContext.h"
#include "Editor/Commands/EditorCommandConcept.h"

namespace Opaax::Editor
{
    // =============================================================================
    // IEditorCommand — the type-erased command: run this action with this payload. Knows nothing about
    //   undo: each action records its own step.
    // =============================================================================
    class IEditorCommand
    {
    private:
        struct Concept
        {
            virtual ~Concept() = default;

            virtual void Execute(EditorContext& Context, const void* Params) = 0;
        };

        template<EditorCommand<EditorContext> T>
        struct Model final : Concept
        {
            explicit Model(T InCommand) : m_Command(std::move(InCommand)) {}

            void Execute(EditorContext& Context, const void* RawParams) override
            {
                m_Command.Execute(Context, *static_cast<const typename T::Params*>(RawParams));
            }

            T m_Command;
        };

    private:
        TUniquePtr<Concept> m_Self;

    public:
        template<EditorCommand<EditorContext> T>
        explicit IEditorCommand(T Command)
            : m_Self(MakeUnique<Model<T>>(std::move(Command)))
        {
        }

        template<typename TParams>
        void Execute(EditorContext& Context, const TParams& Params)
        {
            m_Self->Execute(Context, &Params);
        }
    };
}
