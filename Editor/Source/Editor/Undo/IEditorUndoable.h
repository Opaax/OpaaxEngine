// IEditorUndoable.h
#pragma once

#include <utility>

#include "Core/OpaaxTypes.h"
#include "Editor/Undo/EditorUndoableConcept.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // IEditorUndoable — the type-erased undo step: Undo, Redo, Label.
    //   EditorContext is only forward-declared; Model<T> is instantiated where Record<T>() is called.
    //   Redo restores what was recorded; it never re-runs the action (that would make a new guid, or
    //   reopen a dialog).
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
