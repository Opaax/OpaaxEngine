#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Animation/AnimationClipData.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The undo steps recorded by the clip editor's actions. Plain structs with Undo / Redo / Label.
    // Every step carries the clip's path: undoing after opening another clip does nothing and warns,
    // instead of writing into the wrong file.
    // =============================================================================

    /**
     * The whole step list was replaced (add, remove, reorder). The label says which.
     */
    struct ClipStepsEdit
    {
        OpaaxString              ClipPath;
        TDynArray<AnimationStep> Before;
        TDynArray<AnimationStep> After;

        /** What the Edit menu shows: "Add Step", "Remove Step", "Move Step". */
        const char* LabelText = "Edit Steps";

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return LabelText; }
    };

    /**
     * One step's frame, texture or hold changed (a field in the panel). Bracketed around the gesture,
     * since a property drawer writes directly.
     */
    struct ClipStepEdit
    {
        OpaaxString   ClipPath;
        Uint32        Index = 0;
        AnimationStep Before;
        AnimationStep After;

        /** Stores the step as the before state. */
        void Begin(const EditorContext& InContext, Uint32 InIndex);

        /** @return True when the step differs from what Begin saw. */
        bool End(const EditorContext& InContext);

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Edit Step"; }
    };

    /** The clip's own fields: sheet, fps, play mode. */
    struct ClipSettingsEdit
    {
        OpaaxString   ClipPath;
        AnimationClipData Before;
        AnimationClipData After;

        /** Stores the clip's settings as the before state. */
        void Begin(const EditorContext& InContext);

        /** @return True when any setting differs from what Begin saw. Steps are not compared. */
        bool End(const EditorContext& InContext);

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Edit Clip Settings"; }
    };
}
