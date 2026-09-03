#include "Editor/Undo/AnimationClipUndoables.h"

#include "Editor/EditorAnimationClipDocument.h"
#include "Editor/EditorContext.h"

namespace Opaax::Editor
{
    namespace
    {
        /**
         * The open clip's data when it is the one InPath names, else null WITH A LINE SAYING SO.
         *
         * The stack is not per-document, so a step recorded against one clip can be replayed while
         * another is open. Refusing is the only correct answer; saying nothing would look exactly
         * like an undo that had nothing to do.
         */
        AnimationClipData* TargetClip(EditorContext& InContext, const OpaaxString& InPath)
        {
            if (!InContext.ClipDocument.IsOpen() || InContext.ClipDocument.AbsPath() != InPath)
            {
                OPAAX_LOG(LogEditorAnimationClipDocument, Warn,
                          "Undo step skipped — its clip '{}' is not the open one", InPath.CStr());
                return nullptr;
            }

            return &InContext.ClipDocument.GetMutableData();
        }
    }

    // =============================================================================
    // ClipStepsEdit
    // =============================================================================
    void ClipStepsEdit::Undo(EditorContext& InContext)
    {
        if (AnimationClipData* lData = TargetClip(InContext, ClipPath)) { lData->Steps = Before; }
    }

    void ClipStepsEdit::Redo(EditorContext& InContext)
    {
        if (AnimationClipData* lData = TargetClip(InContext, ClipPath)) { lData->Steps = After; }
    }

    // =============================================================================
    // ClipStepEdit
    // =============================================================================
    void ClipStepEdit::Begin(const EditorContext& InContext, const Uint32 InIndex)
    {
        const AnimationClipData& lData = InContext.ClipDocument.GetData();

        ClipPath = InContext.ClipDocument.AbsPath();
        Index    = InIndex;
        Before   = (InIndex < lData.StepCount()) ? lData.Steps[InIndex] : AnimationStep{};
        After    = Before;
    }

    bool ClipStepEdit::End(const EditorContext& InContext)
    {
        const AnimationClipData& lData = InContext.ClipDocument.GetData();

        if (ClipPath != InContext.ClipDocument.AbsPath() || Index >= lData.StepCount())
        {
            return false;   // the clip changed under the gesture — there is no step to record
        }

        After = lData.Steps[Index];

        // A gesture that changed nothing is not a step. Field by field rather than through the
        // serializer: this runs on release, and a json dump would be work done to answer a
        // question three fields already answer.
        return After.Frame != Before.Frame
            || After.Hold  != Before.Hold
            || After.Texture.Path != Before.Texture.Path;
    }

    void ClipStepEdit::Undo(EditorContext& InContext)
    {
        if (AnimationClipData* lData = TargetClip(InContext, ClipPath);
            lData != nullptr && Index < lData->StepCount())
        {
            lData->Steps[Index] = Before;
        }
    }

    void ClipStepEdit::Redo(EditorContext& InContext)
    {
        if (AnimationClipData* lData = TargetClip(InContext, ClipPath);
            lData != nullptr && Index < lData->StepCount())
        {
            lData->Steps[Index] = After;
        }
    }

    // =============================================================================
    // ClipSettingsEdit
    // =============================================================================
    void ClipSettingsEdit::Begin(const EditorContext& InContext)
    {
        ClipPath = InContext.ClipDocument.AbsPath();
        Before   = InContext.ClipDocument.GetData();
        After    = Before;
    }

    bool ClipSettingsEdit::End(const EditorContext& InContext)
    {
        if (ClipPath != InContext.ClipDocument.AbsPath())
        {
            return false;
        }

        After = InContext.ClipDocument.GetData();

        // The STEPS are deliberately not compared: they are ClipStepsEdit's and ClipStepEdit's, and
        // a settings gesture that happened to overlap one must not swallow it.
        return After.Fps        != Before.Fps
            || After.PlayMode   != Before.PlayMode
            || After.Sheet.Path != Before.Sheet.Path;
    }

    void ClipSettingsEdit::Undo(EditorContext& InContext)
    {
        if (AnimationClipData* lData = TargetClip(InContext, ClipPath))
        {
            // Only the settings — restoring Before wholesale would revert step edits made since.
            lData->Fps      = Before.Fps;
            lData->PlayMode = Before.PlayMode;
            lData->Sheet    = Before.Sheet;
        }
    }

    void ClipSettingsEdit::Redo(EditorContext& InContext)
    {
        if (AnimationClipData* lData = TargetClip(InContext, ClipPath))
        {
            lData->Fps      = After.Fps;
            lData->PlayMode = After.PlayMode;
            lData->Sheet    = After.Sheet;
        }
    }
}
