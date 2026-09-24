#include "Editor/Operation/ClipOperations.h"
#include "Editor/Operation/ResourceOperations.h"

#include "Editor/Resources/Types/Animation/EditorAnimationClipDocument.h"
#include "Editor/EditorContext.h"
#include "Editor/Undo/AnimationClipUndoables.h"
#include "Editor/Undo/EditorUndo.h"

#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationClipFile.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationClipResource.h"

namespace Opaax::Editor
{
    namespace
    {
        /** Record a whole-list replacement under InLabel, and publish it into the document. */
        void RecordSteps(EditorContext& InContext, TDynArray<AnimationStep> InAfter, const char* InLabel)
        {
            AnimationClipData& lData = InContext.ClipDocument.GetMutableData();

            ClipStepsEdit lStep;
            lStep.ClipPath  = InContext.ClipDocument.AbsPath();
            lStep.Before    = lData.Steps;
            lStep.After     = InAfter;
            lStep.LabelText = InLabel;

            lData.Steps = Move(InAfter);

            InContext.Undo.Record(Move(lStep));
        }
    }

    bool ClipOps::AddStep(EditorContext& InContext)
    {
        if (!InContext.ClipDocument.IsOpen()) { return false; }

        const AnimationClipData& lData = InContext.ClipDocument.GetData();

        TDynArray<AnimationStep> lAfter = lData.Steps;

        // Copy the LAST step rather than appending a blank one: a run of frames is authored by
        // adding and retargeting, so inheriting the hold and the texture is what saves the typing.
        lAfter.emplace_back(lAfter.empty() ? AnimationStep{} : lAfter.back());

        RecordSteps(InContext, Move(lAfter), "Add Step");

        OPAAX_LOG(LogEditorAnimationClipDocument, Info, "Added step {}",
                  InContext.ClipDocument.GetData().StepCount() - 1u);
        return true;
    }

    bool ClipOps::AddStepWithTexture(EditorContext& InContext, const OpaaxString& InAssetPath)
    {
        if (!InContext.ClipDocument.IsOpen() || InAssetPath.IsEmpty()) { return false; }

        const AnimationClipData& lData = InContext.ClipDocument.GetData();

        TDynArray<AnimationStep> lAfter = lData.Steps;

        AnimationStep lStep;
        lStep.Texture.Path = InAssetPath;
        lStep.Hold         = lAfter.empty() ? 1u : lAfter.back().EffectiveHold();

        lAfter.emplace_back(Move(lStep));

        RecordSteps(InContext, Move(lAfter), "Add Step");

        OPAAX_LOG(LogEditorAnimationClipDocument, Info, "Added step {} showing '{}'",
                  InContext.ClipDocument.GetData().StepCount() - 1u, InAssetPath.CStr());
        return true;
    }

    bool ClipOps::RemoveStep(EditorContext& InContext, const Uint32 InIndex)
    {
        if (!InContext.ClipDocument.IsOpen()) { return false; }

        const AnimationClipData& lData = InContext.ClipDocument.GetData();

        if (InIndex >= lData.StepCount()) { return false; }

        TDynArray<AnimationStep> lAfter = lData.Steps;
        lAfter.erase(lAfter.begin() + InIndex);

        RecordSteps(InContext, Move(lAfter), "Remove Step");
        return true;
    }

    bool ClipOps::MoveStep(EditorContext& InContext, const Uint32 InIndex, const Int32 InDelta)
    {
        if (!InContext.ClipDocument.IsOpen()) { return false; }

        const AnimationClipData& lData = InContext.ClipDocument.GetData();

        if (InIndex >= lData.StepCount()) { return false; }

        const Int32 lTargetRaw = static_cast<Int32>(InIndex) + InDelta;
        const Int32 lLast      = static_cast<Int32>(lData.StepCount()) - 1;
        const Int32 lTarget    = (lTargetRaw < 0) ? 0 : ((lTargetRaw > lLast) ? lLast : lTargetRaw);

        // Clamped rather than refused, so holding the button at the end of the list is quiet — but
        // a move that lands where it started is NOT a step.
        if (lTarget == static_cast<Int32>(InIndex)) { return false; }

        TDynArray<AnimationStep> lAfter = lData.Steps;
        const AnimationStep      lMoved = lAfter[InIndex];

        lAfter.erase(lAfter.begin() + InIndex);
        lAfter.insert(lAfter.begin() + lTarget, lMoved);

        RecordSteps(InContext, Move(lAfter), "Move Step");
        return true;
    }

    bool ClipOps::SetStepFrame(EditorContext& InContext, const Uint32 InIndex, const OpaaxStringID InFrame)
    {
        if (!InContext.ClipDocument.IsOpen()) { return false; }

        AnimationClipData& lData = InContext.ClipDocument.GetMutableData();

        if (InIndex >= lData.StepCount() || lData.Steps[InIndex].Frame == InFrame) { return false; }

        ClipStepEdit lStep;
        lStep.Begin(InContext, InIndex);

        lData.Steps[InIndex].Frame = InFrame;

        if (lStep.End(InContext))
        {
            InContext.Undo.Record(Move(lStep));
        }

        return true;
    }

    bool ClipOps::Save(EditorContext& InContext)
    {
        if (!InContext.ClipDocument.IsOpen()) { return false; }

        if (!AnimationClipFile::Save(InContext.ClipDocument.AbsPath(), InContext.ClipDocument.GetData()))
        {
            return false;   // AnimationClipFile logged why
        }

        // Only after a SUCCESSFUL write: rebasing on a failed save would clear the dirty marker
        // while the file still holds the old content — the marker lying is worse than the failure.
        InContext.ClipDocument.MarkSaved();

        // AND PUBLISH IT. Without this an entity already playing this clip keeps playing whatever
        // the ResourceManager parsed first, so editing the file changes nothing on screen — the
        // exact defect [[L75]] records. Reload swaps the payload in place, so every ResourceRef
        // already held stays valid and simply sees the new steps.
        //
        // Not resident is the ordinary case (nothing plays this clip yet) and answers false, so the
        // result is deliberately not treated as a failure of the save.
        ResourceOps::SavedToDisk<AnimationClipResource>(InContext, InContext.ClipDocument.AbsPath());

        return true;
    }
}
