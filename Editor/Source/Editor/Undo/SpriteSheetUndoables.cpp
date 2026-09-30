#include "Editor/Undo/SpriteSheetUndoables.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorSpriteSheetDocument.h"

namespace Opaax::Editor
{
    namespace
    {
        /**
         * The open sheet's data if it is the one InPath names, else null (and a warning). The stack is not
         * per document, so a step can be replayed while another sheet is open.
         */
        SpriteSheetData* TargetSheet(EditorContext& InContext, const OpaaxString& InPath)
        {
            if (!InContext.SheetDocument.IsOpen() || InContext.SheetDocument.AbsPath() != InPath)
            {
                OPAAX_LOG(LogEditorSpriteSheetDocument, Warn,
                          "Undo step skipped — its sheet '{}' is not the open one", InPath.CStr());
                return nullptr;
            }

            return &InContext.SheetDocument.GetMutableData();
        }
    }

    // =============================================================================
    // SheetSlice
    // =============================================================================
    void SheetSlice::Undo(EditorContext& InContext)
    {
        if (SpriteSheetData* lData = TargetSheet(InContext, SheetPath)) { lData->Frames = Before; }
    }

    void SheetSlice::Redo(EditorContext& InContext)
    {
        if (SpriteSheetData* lData = TargetSheet(InContext, SheetPath)) { lData->Frames = After; }
    }

    // =============================================================================
    // SheetAutoName
    // =============================================================================
    void SheetAutoName::Undo(EditorContext& InContext)
    {
        if (SpriteSheetData* lData = TargetSheet(InContext, SheetPath)) { lData->Frames = Before; }
    }

    void SheetAutoName::Redo(EditorContext& InContext)
    {
        if (SpriteSheetData* lData = TargetSheet(InContext, SheetPath)) { lData->Frames = After; }
    }

    // =============================================================================
    // SheetFrameEdit
    // =============================================================================
    void SheetFrameEdit::Begin(const EditorContext& InContext, const Uint32 InIndex)
    {
        const SpriteSheetData& lData = InContext.SheetDocument.GetData();

        SheetPath = InContext.SheetDocument.AbsPath();
        Index     = InIndex;
        Before    = (InIndex < lData.FrameCount()) ? lData.Frames[InIndex] : SpriteFrame{};
        After     = Before;
    }

    bool SheetFrameEdit::End(const EditorContext& InContext)
    {
        const SpriteSheetData& lData = InContext.SheetDocument.GetData();

        if (SheetPath != InContext.SheetDocument.AbsPath() || Index >= lData.FrameCount())
        {
            return false;   // the sheet changed during the gesture: no step
        }

        After = lData.Frames[Index];

        // A gesture that moved nothing is not a step (compared field by field, cheaper than serializing).
        return After.Name       != Before.Name
            || After.Offset.x   != Before.Offset.x || After.Offset.y != Before.Offset.y
            || After.Size.x     != Before.Size.x   || After.Size.y   != Before.Size.y;
    }

    void SheetFrameEdit::Undo(EditorContext& InContext)
    {
        if (SpriteSheetData* lData = TargetSheet(InContext, SheetPath);
            lData != nullptr && Index < lData->FrameCount())
        {
            lData->Frames[Index] = Before;
        }
    }

    void SheetFrameEdit::Redo(EditorContext& InContext)
    {
        if (SpriteSheetData* lData = TargetSheet(InContext, SheetPath);
            lData != nullptr && Index < lData->FrameCount())
        {
            lData->Frames[Index] = After;
        }
    }

    // =============================================================================
    // SheetDefaultFrame
    // =============================================================================
    void SheetDefaultFrame::Undo(EditorContext& InContext)
    {
        if (SpriteSheetData* lData = TargetSheet(InContext, SheetPath)) { lData->DefaultFrame = Before; }
    }

    void SheetDefaultFrame::Redo(EditorContext& InContext)
    {
        if (SpriteSheetData* lData = TargetSheet(InContext, SheetPath)) { lData->DefaultFrame = After; }
    }
}
