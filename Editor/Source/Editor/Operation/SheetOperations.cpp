#include "Editor/Operation/SheetOperations.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorSpriteSheetDocument.h"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/Undo/SpriteSheetUndoables.h"

#include "Engine/Subsystems/Resources/Types/SpriteSheetFile.h"

namespace Opaax::Editor
{
    bool SheetOps::Slice(EditorContext& InContext, const Uint32 InTexWidth, const Uint32 InTexHeight)
    {
        if (!InContext.SheetDocument.IsOpen()) { return false; }

        SpriteSheetData& lData = InContext.SheetDocument.GetMutableData();

        TDynArray<SpriteFrame> lSliced = SliceGrid(lData.Grid, InTexWidth, InTexHeight);

        // A grid that cuts nothing is a refusal, not a result: replacing an authored frame list
        // with an empty one because the cell size was mistyped is data loss with an undo step.
        if (lSliced.empty())
        {
            OPAAX_LOG(LogEditorSpriteSheetDocument, Warn,
                      "Slice produced no frames for a {}x{} texture — the frames are unchanged",
                      InTexWidth, InTexHeight);
            return false;
        }

        SheetSlice lStep;
        lStep.SheetPath = InContext.SheetDocument.AbsPath();
        lStep.Before    = lData.Frames;
        lStep.After     = lSliced;

        lData.Frames = Move(lSliced);

        // The default may now name a frame that no longer exists — clamped here rather than left
        // for a reader to trip over. It rides in the same step, so one Ctrl+Z puts both back.
        if (lData.DefaultFrame >= lData.FrameCount()) { lData.DefaultFrame = 0; }

        InContext.Undo.Record(Move(lStep));

        OPAAX_LOG(LogEditorSpriteSheetDocument, Info, "Sliced into {} frame(s)", lData.FrameCount());
        return true;
    }

    bool SheetOps::SetDefaultFrame(EditorContext& InContext, const Uint32 InIndex)
    {
        if (!InContext.SheetDocument.IsOpen()) { return false; }

        SpriteSheetData& lData = InContext.SheetDocument.GetMutableData();

        if (InIndex >= lData.FrameCount() || InIndex == lData.DefaultFrame) { return false; }

        SheetDefaultFrame lStep;
        lStep.SheetPath = InContext.SheetDocument.AbsPath();
        lStep.Before    = lData.DefaultFrame;
        lStep.After     = InIndex;

        lData.DefaultFrame = InIndex;

        InContext.Undo.Record(Move(lStep));
        return true;
    }

    bool SheetOps::Save(EditorContext& InContext)
    {
        if (!InContext.SheetDocument.IsOpen()) { return false; }

        if (!SpriteSheetFile::Save(InContext.SheetDocument.AbsPath(), InContext.SheetDocument.GetData()))
        {
            return false;   // SpriteSheetFile logged why
        }

        // Only after a SUCCESSFUL write: rebasing on a failed save would clear the dirty marker
        // while the file still holds the old content — the marker lying is worse than the failure.
        InContext.SheetDocument.MarkSaved();
        return true;
    }
}
