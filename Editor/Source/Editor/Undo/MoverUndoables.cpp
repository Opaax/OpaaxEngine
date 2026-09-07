#include "Editor/Undo/MoverUndoables.h"

#include "Editor/EditorContext.h"
#include "Editor/Resources/Types/Mover/EditorMoveModeDocument.h"
#include "Editor/Resources/Types/Mover/EditorMoverDocument.h"

namespace Opaax::Editor
{
    namespace
    {
        /** The open mover's data when it is the one InPath names, else null WITH A LINE SAYING SO. */
        MoverData* TargetMover(EditorContext& InContext, const OpaaxString& InPath)
        {
            if (!InContext.MoverDocument.IsOpen() || InContext.MoverDocument.AbsPath() != InPath)
            {
                OPAAX_LOG(LogEditorMoverDocument, Warn,
                          "Undo step skipped — its mover '{}' is not the open one", InPath.CStr());
                return nullptr;
            }

            return &InContext.MoverDocument.GetMutableData();
        }

        /** The open tuning's data when it is the one InPath names, else null WITH A LINE SAYING SO. */
        MoveModeData* TargetMode(EditorContext& InContext, const OpaaxString& InPath)
        {
            if (!InContext.MoveModeDocument.IsOpen() || InContext.MoveModeDocument.AbsPath() != InPath)
            {
                OPAAX_LOG(LogEditorMoveModeDocument, Warn,
                          "Undo step skipped — its move mode '{}' is not the open one", InPath.CStr());
                return nullptr;
            }

            return &InContext.MoveModeDocument.GetMutableData();
        }
    }

    // =============================================================================
    // MoverEntriesEdit
    // =============================================================================
    void MoverEntriesEdit::Undo(EditorContext& InContext)
    {
        if (MoverData* lData = TargetMover(InContext, MoverPath))
        {
            lData->Entries     = Before;
            lData->DefaultMode = BeforeDefault;
        }
    }

    void MoverEntriesEdit::Redo(EditorContext& InContext)
    {
        if (MoverData* lData = TargetMover(InContext, MoverPath))
        {
            lData->Entries     = After;
            lData->DefaultMode = AfterDefault;
        }
    }

    // =============================================================================
    // MoverDefaultMode
    // =============================================================================
    void MoverDefaultMode::Undo(EditorContext& InContext)
    {
        if (MoverData* lData = TargetMover(InContext, MoverPath)) { lData->DefaultMode = Before; }
    }

    void MoverDefaultMode::Redo(EditorContext& InContext)
    {
        if (MoverData* lData = TargetMover(InContext, MoverPath)) { lData->DefaultMode = After; }
    }

    // =============================================================================
    // MoveModeEdit
    // =============================================================================
    void MoveModeEdit::Undo(EditorContext& InContext)
    {
        if (MoveModeData* lData = TargetMode(InContext, ModePath)) { *lData = Before; }
    }

    void MoveModeEdit::Redo(EditorContext& InContext)
    {
        if (MoveModeData* lData = TargetMode(InContext, ModePath)) { *lData = After; }
    }
}
