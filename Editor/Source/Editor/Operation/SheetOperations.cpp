#include "Editor/Operation/SheetOperations.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorSpriteSheetDocument.h"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/Undo/SpriteSheetUndoables.h"

#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/Types/SpriteSheetFile.h"
#include "Engine/Subsystems/Resources/Types/SpriteSheetResource.h"

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

    namespace
    {
        /** Whether any frame already carries InName — the uniqueness half of AutoNameFrames. */
        bool NameInUse(const TDynArray<SpriteFrame>& InFrames, const OpaaxStringID InName)
        {
            for (const SpriteFrame& lFrame : InFrames)
            {
                if (lFrame.Name == InName) { return true; }
            }

            return false;
        }
    }

    bool SheetOps::AutoNameFrames(EditorContext& InContext)
    {
        if (!InContext.SheetDocument.IsOpen()) { return false; }

        SpriteSheetData& lData = InContext.SheetDocument.GetMutableData();

        TDynArray<SpriteFrame> lNamed  = lData.Frames;
        Uint32                 lFilled = 0;

        for (Uint32 lIndex = 0; lIndex < lNamed.size(); ++lIndex)
        {
            // An authored name is never overwritten. That is what makes this button safe to press
            // twice, and safe to press on a sheet somebody has already named by hand.
            if (lNamed[lIndex].Name.IsValid()) { continue; }

            // Step past any name already in use rather than producing a duplicate.
            OpaaxStringID lCandidate;

            for (Uint32 lSuffix = lIndex; ; ++lSuffix)
            {
                lCandidate = OpaaxStringID(OpaaxString("Frame_") + OpaaxString::FromUInt(lSuffix));

                if (!NameInUse(lNamed, lCandidate)) { break; }
            }

            lNamed[lIndex].Name = lCandidate;
            ++lFilled;
        }

        if (lFilled == 0)
        {
            OPAAX_LOG(LogEditorSpriteSheetDocument, Info,
                      "Every frame is already named — nothing to do");
            return false;
        }

        SheetAutoName lStep;
        lStep.SheetPath = InContext.SheetDocument.AbsPath();
        lStep.Before    = lData.Frames;
        lStep.After     = lNamed;

        lData.Frames = Move(lNamed);

        InContext.Undo.Record(Move(lStep));

        OPAAX_LOG(LogEditorSpriteSheetDocument, Info, "Named {} of {} frame(s)",
                  lFilled, lData.FrameCount());
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

        // AND PUBLISH IT. The editor edits its own copy, so without this the renderer keeps drawing
        // whatever the ResourceManager loaded the first time a sprite named this sheet — re-slicing
        // changed the file and nothing on screen. Reload swaps the payload in place, so every
        // ResourceRef already held stays valid and simply sees the new frames.
        //
        // Not resident is the ordinary case (no sprite uses this sheet yet) and answers false, so
        // the result is deliberately not treated as a failure of the save.
        InContext.Resources.Reload<SpriteSheetResource>(InContext.SheetDocument.AbsPath().CStr());

        return true;
    }
}
