#include "Editor/Operation/SheetOperations.h"
#include "Editor/Operation/ResourceOperations.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorSpriteSheetDocument.h"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/Undo/SpriteSheetUndoables.h"

#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/Types/SpriteSheet/SpriteSheetFile.h"
#include "Engine/Subsystems/Resources/Types/SpriteSheet/SpriteSheetResource.h"

namespace Opaax::Editor
{
    bool SheetOps::Slice(EditorContext& InContext, const Uint32 InTexWidth, const Uint32 InTexHeight)
    {
        if (!InContext.SheetDocument.IsOpen()) { return false; }

        SpriteSheetData& lData = InContext.SheetDocument.GetMutableData();

        TDynArray<SpriteFrame> lSliced = SliceGrid(lData.Grid, InTexWidth, InTexHeight);

        // A grid that cuts nothing is refused (a mistyped cell size would wipe the frames).
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

        // The default may name a frame that no longer exists: clamp it, in the same undo step.
        if (lData.DefaultFrame >= lData.FrameCount()) { lData.DefaultFrame = 0; }

        InContext.Undo.Record(Move(lStep));

        OPAAX_LOG(LogEditorSpriteSheetDocument, Info, "Sliced into {} frame(s)", lData.FrameCount());
        return true;
    }

    namespace
    {
        /** Whether any frame already has InName. */
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
            // A name set by the author is never overwritten (safe to press twice).
            if (lNamed[lIndex].Name.IsValid()) { continue; }

            // Skip names already in use.
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

        // Only after a successful write: otherwise the dirty marker would lie.
        InContext.SheetDocument.MarkSaved();

        // Reload the resource, so sprites already using this sheet see the new frames. Not loaded is
        // normal (no sprite uses it yet) and is not a failure.
        ResourceOps::SavedToDisk<SpriteSheetResource>(InContext, InContext.SheetDocument.AbsPath());

        return true;
    }
}
