#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // SheetOps — the sprite sheet editor's VERBS, beside MapOps and LevelOps.
    //
    //   Each one mutates the open document and records its own undo step, which is the whole reason
    //   they are verbs rather than panel code: **UN1** says whoever makes the edit builds the step,
    //   so a second caller (a menu entry, a shortcut, a game's editor extension) cannot forget to.
    //
    //   The panel keeps only what a panel owns — the selection, the drag in progress — and calls
    //   these for anything that changes the sheet.
    // =============================================================================
    namespace SheetOps
    {
        /**
         * Replace the frame list with what the sheet's grid cuts out of its texture.
         *
         * Needs the texture's SIZE, which the document does not hold: a sheet stores a path, and
         * how many pixels that is, is the image's answer. The panel has it loaded already.
         *
         * @return false when there is no sheet open, or the grid produces nothing (a cell larger
         *   than the texture, a zero cell) — refusing beats replacing the frames with an empty list.
         */
        bool Slice(EditorContext& InContext, Uint32 InTexWidth, Uint32 InTexHeight);

        /**
         * Give every UNNAMED frame a name — "Frame_0", "Frame_1", … — leaving authored ones alone.
         *
         * SliceGrid deliberately generates unnamed frames, and an animation step references a frame
         * BY NAME, so a freshly sliced sheet has nothing a clip can point at. This is the one click
         * that closes that gap; without it a 64-frame sheet is 64 renames before the first clip.
         *
         * Non-destructive on purpose (it only fills blanks) and it guarantees UNIQUENESS, because
         * the animation binder resolves a name by first match — two frames sharing one would
         * silently animate the wrong picture.
         *
         * @return false when nothing is open or every frame was already named; the second says so
         *   in the log rather than recording an undo step that changes nothing.
         */
        bool AutoNameFrames(EditorContext& InContext);

        /** Point the sheet's DefaultFrame at InIndex. No-op when it already is, or is out of range. */
        bool SetDefaultFrame(EditorContext& InContext, Uint32 InIndex);

        /**
         * Write the open sheet back to its file and rebase the dirty marker.
         *
         * @return false when nothing is open or the write failed — SpriteSheetFile logs which.
         */
        bool Save(EditorContext& InContext);
    }
}
