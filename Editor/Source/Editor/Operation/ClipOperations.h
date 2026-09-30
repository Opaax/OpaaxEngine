#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // ClipOps — the animation clip editor's actions. Each changes the open document and records its
    //   own undo step, so every caller (panel, menu, shortcut, game extension) gets undo.
    // =============================================================================
    namespace ClipOps
    {
        /**
         * Adds a step, copying the last one.
         * @return False when no clip is open
         */
        bool AddStep(EditorContext& InContext);

        /**
         * Adds a step showing InAssetPath (a texture dropped on the step list). One undo step per drop.
         * @return False when nothing is open or the path is empty
         */
        bool AddStepWithTexture(EditorContext& InContext, const OpaaxString& InAssetPath);

        /** Removes the step at InIndex. @return False when nothing is open or the index is out of range */
        bool RemoveStep(EditorContext& InContext, Uint32 InIndex);

        /**
         * Moves the step at InIndex by InDelta places, clamped to the list.
         * @return False when it would not move (no undo step)
         */
        bool MoveStep(EditorContext& InContext, Uint32 InIndex, Int32 InDelta);

        /** Points a step at a sheet frame, by name. @return False when nothing changed */
        bool SetStepFrame(EditorContext& InContext, Uint32 InIndex, OpaaxStringID InFrame);

        /**
         * Writes the open clip to its file, updates the dirty baseline, and reloads the resource (so
         * entities already playing it see the change).
         * @return False when nothing is open or the write failed (AnimationClipFile logs why)
         */
        bool Save(EditorContext& InContext);
    }
}
