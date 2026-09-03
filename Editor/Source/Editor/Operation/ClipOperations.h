#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // ClipOps — the animation clip editor's VERBS, beside SheetOps, MapOps and LevelOps.
    //
    //   Each one mutates the open document and records its own undo step, which is the whole reason
    //   they are verbs rather than panel code: **UN1** says whoever makes the edit builds the step,
    //   so a second caller (a menu entry, a shortcut, a game's editor extension) cannot forget to.
    // =============================================================================
    namespace ClipOps
    {
        /**
         * Append a step, copying the last one so a run of frames is "add, retarget" rather than
         * "add, then set every field".
         *
         * @return false when no clip is open.
         */
        bool AddStep(EditorContext& InContext);

        /** Remove the step at InIndex. @return false when nothing is open or the index is past the end. */
        bool RemoveStep(EditorContext& InContext, Uint32 InIndex);

        /**
         * Move the step at InIndex by InDelta places, clamped to the list.
         *
         * @return false when the move would leave it where it is — a no-op is not an undo step.
         */
        bool MoveStep(EditorContext& InContext, Uint32 InIndex, Int32 InDelta);

        /** Point a step at a sheet frame BY NAME. @return false when nothing changed. */
        bool SetStepFrame(EditorContext& InContext, Uint32 InIndex, OpaaxStringID InFrame);

        /**
         * Write the open clip back to its file and rebase the dirty marker.
         *
         * It also RELOADS the resource, which is not optional: the editor edits its own copy, so
         * without it an entity already playing this clip keeps playing the first parse ([[L75]]).
         *
         * @return false when nothing is open or the write failed — AnimationClipFile logs which.
         */
        bool Save(EditorContext& InContext);
    }
}
