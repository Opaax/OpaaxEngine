#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationClipData.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The steps the clip editor's verbs record. UN1's shape: a struct with Undo / Redo / Label, no
    // base class and no registration, built by whoever made the edit.
    //
    // EVERY STEP CARRIES THE CLIP'S PATH, for SpriteSheetUndoables' reason: one clip is open at a
    // time and the undo stack outlives that, so without it an undo after opening a second clip
    // would write the first one's steps into it. A step whose clip is not the open one is a NO-OP
    // WITH A WARNING — loud, because a silently skipped undo looks exactly like one that had
    // nothing to do.
    // =============================================================================

    /**
     * The whole step LIST was replaced — add, remove and reorder are all this.
     *
     * One type for three verbs because they are one edit to a reader: the list before, the list
     * after. Splitting them would buy three labels and three copies of the same two lines, so the
     * LABEL is the field that varies instead.
     */
    struct ClipStepsEdit
    {
        OpaaxString              ClipPath;
        TDynArray<AnimationStep> Before;
        TDynArray<AnimationStep> After;

        /** What the Edit menu shows — "Add Step", "Remove Step", "Move Step". */
        const char* LabelText = "Edit Steps";

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return LabelText; }
    };

    /**
     * ONE step's frame, texture or hold changed — a field in the panel.
     *
     * The gesture pattern SheetFrameEdit already uses, and for the same reason: a property drawer
     * writes straight through a reference and cannot report that it did, so the step is bracketed
     * around the gesture rather than recorded per mutation.
     */
    struct ClipStepEdit
    {
        OpaaxString   ClipPath;
        Uint32        Index = 0;
        AnimationStep Before;
        AnimationStep After;

        /** Cache the step as the BEFORE half. */
        void Begin(const EditorContext& InContext, Uint32 InIndex);

        /** @return true when the step differs from what Begin saw. */
        bool End(const EditorContext& InContext);

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Edit Step"; }
    };

    /** The clip's own fields — its sheet, its fps, its play mode. */
    struct ClipSettingsEdit
    {
        OpaaxString   ClipPath;
        AnimationClipData Before;
        AnimationClipData After;

        /** Cache the clip's settings as the BEFORE half. */
        void Begin(const EditorContext& InContext);

        /** @return true when any setting differs from what Begin saw. The STEPS are not compared. */
        bool End(const EditorContext& InContext);

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Edit Clip Settings"; }
    };
}
