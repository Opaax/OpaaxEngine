// IEditorUndoable.h
#pragma once

#include <utility>

#include "Core/OpaaxTypes.h"
#include "Editor/Undo/EditorUndoableConcept.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // IEditorUndoable — the type-erased undo step.
    //
    //   Three virtuals, because a step is three questions: put it back, put it forward, what is it
    //   called. EditorContext is only FORWARD-DECLARED here — Model<T>'s bodies are instantiated at
    //   the Record<T>() call site, where the context is complete — which is what keeps the stack
    //   free of every header the steps themselves need.
    //
    //   REDO IS NEVER A SECOND EXECUTE. A step re-asserts what it recorded; re-running the verb
    //   would mint a fresh Guid (create) or re-open a dialog (save-as).
    // =============================================================================
    class IEditorUndoable
    {
    private:
        struct Concept
        {
            virtual ~Concept() = default;

            virtual void        Undo(EditorContext& Context) = 0;
            virtual void        Redo(EditorContext& Context) = 0;
            virtual const char* Label() const = 0;
        };

        template<EditorUndoable<EditorContext> T>
        struct Model final : Concept
        {
            explicit Model(T InStep) : m_Step(std::move(InStep)) {}

            void        Undo(EditorContext& Context) override { m_Step.Undo(Context); }
            void        Redo(EditorContext& Context) override { m_Step.Redo(Context); }
            const char* Label() const override { return m_Step.Label(); }

            T m_Step;
        };

    public:
        template<EditorUndoable<EditorContext> T>
        explicit IEditorUndoable(T InStep)
            : m_Self(MakeUnique<Model<T>>(std::move(InStep)))
        {
        }

        void        Undo(EditorContext& Context) { m_Self->Undo(Context); }
        void        Redo(EditorContext& Context) { m_Self->Redo(Context); }
        const char* Label() const { return m_Self->Label(); }

    private:
        TUniquePtr<Concept> m_Self;
    };
}
