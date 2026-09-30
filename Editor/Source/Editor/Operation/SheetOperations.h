#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // SheetOps — the sprite sheet editor's actions. Each changes the open document and records its
    //   own undo step. The panel keeps only its selection and current drag.
    // =============================================================================
    namespace SheetOps
    {
        /**
         * Replaces the frame list with the sheet's grid cut over its texture. The texture size comes from
         * the caller (the panel has it loaded).
         * @return False when no sheet is open, or the grid produces nothing (refused instead of emptying
         *   the frames)
         */
        bool Slice(EditorContext& InContext, Uint32 InTexWidth, Uint32 InTexHeight);

        /**
         * Names every unnamed frame ("Frame_0", "Frame_1", ...), leaving named ones alone. Names are unique
         * (clips reference frames by name, first match).
         * @return False when nothing is open or every frame already has a name (logged, no undo step)
         */
        bool AutoNameFrames(EditorContext& InContext);

        /** Sets the sheet's DefaultFrame to InIndex. Does nothing when already set or out of range. */
        bool SetDefaultFrame(EditorContext& InContext, Uint32 InIndex);

        /**
         * Writes the open sheet to its file and updates the dirty baseline.
         * @return False when nothing is open or the write failed (SpriteSheetFile logs why)
         */
        bool Save(EditorContext& InContext);
    }
}
