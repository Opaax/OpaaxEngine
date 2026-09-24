#include "Editor/Undo/EditorUndo.h"

#include "Application/Services/ILogger.h"

using namespace Opaax;   // OPAAX_LOG expands to an unqualified ToSpdLevel(...)

namespace
{
    constexpr LogCategory LogEditorUndo{"EditorUndo"};
}

namespace Opaax::Editor
{
    void EditorUndo::Push(IEditorUndoable&& InStep)
    {
        m_Redo.clear();
        m_Undo.emplace_back(Move(InStep));

        if (m_Undo.size() > MAX_UNDO_STEPS)
        {
            m_Undo.erase(m_Undo.begin());
        }

        // A NUMBER, not "it worked": this is what says a whole drag landed as ONE step, which no
        // screenshot of the viewport can show.
        OPAAX_LOG(LogEditorUndo, Info, "Recorded '{}' - undo depth {}",
                  m_Undo.back().Label(), static_cast<Uint64>(m_Undo.size()));
    }

    const char* EditorUndo::UndoLabel() const noexcept
    {
        return m_Undo.empty() ? "" : m_Undo.back().Label();
    }

    const char* EditorUndo::RedoLabel() const noexcept
    {
        return m_Redo.empty() ? "" : m_Redo.back().Label();
    }

    void EditorUndo::Undo(EditorContext& InContext)
    {
        if (!CanUndo())
        {
            // SAYS SO. A silent return made "Ctrl+Z did nothing" indistinguishable from three
            // different causes — the chord never fired, the command was gated, or no step was ever
            // recorded — which is exactly the ambiguity that made a real report undiagnosable.
            OPAAX_LOG(LogEditorUndo, Info, "Undo - nothing to undo ({} step(s) redoable)",
                      static_cast<Uint64>(m_Redo.size()));
            return;
        }

        IEditorUndoable lStep = Move(m_Undo.back());
        m_Undo.pop_back();

        m_bApplying = true;
        lStep.Undo(InContext);
        m_bApplying = false;

        OPAAX_LOG(LogEditorUndo, Info, "Undo '{}' - {} step(s) left, {} to redo",
                  lStep.Label(), static_cast<Uint64>(m_Undo.size()),
                  static_cast<Uint64>(m_Redo.size() + 1));

        m_Redo.emplace_back(Move(lStep));
    }

    void EditorUndo::Redo(EditorContext& InContext)
    {
        if (!CanRedo())
        {
            OPAAX_LOG(LogEditorUndo, Info, "Redo - nothing to redo ({} step(s) undoable)",
                      static_cast<Uint64>(m_Undo.size()));
            return;
        }

        IEditorUndoable lStep = Move(m_Redo.back());
        m_Redo.pop_back();

        m_bApplying = true;
        lStep.Redo(InContext);
        m_bApplying = false;

        OPAAX_LOG(LogEditorUndo, Info, "Redo '{}' - {} step(s) to undo, {} left",
                  lStep.Label(), static_cast<Uint64>(m_Undo.size() + 1),
                  static_cast<Uint64>(m_Redo.size()));

        m_Undo.emplace_back(Move(lStep));
    }

    void EditorUndo::Clear() noexcept
    {
        m_Undo.clear();
        m_Redo.clear();
    }
}
