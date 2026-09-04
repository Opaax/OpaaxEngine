// SpriteSheetUndoables.h
#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/Types/SpriteSheet/SpriteSheetData.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The steps the sheet editor's three verbs record. UN1's shape: a struct with Undo / Redo /
    // Label, no base class and no registration, built by whoever made the edit.
    //
    // EVERY STEP CARRIES THE SHEET'S PATH, which the entity steps have no equivalent of. One sheet
    // is open at a time and the undo stack outlives that: without the path, undoing after opening a
    // second sheet would write the first one's frames into it. A step whose sheet is not the open
    // one is a NO-OP WITH A WARNING — loud, because a silently skipped undo is indistinguishable
    // from one that did nothing because there was nothing to do.
    // =============================================================================

    /** The whole frame list was replaced — a slice is ONE act, however many frames it produced. */
    struct SheetSlice
    {
        OpaaxString            SheetPath;
        TDynArray<SpriteFrame> Before;
        TDynArray<SpriteFrame> After;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Slice Sheet"; }
    };

    /**
     * Every unnamed frame got a name — ONE act, however many it filled.
     *
     * Its own type rather than a SheetSlice, because the Edit menu names the step ("Undo Auto-Name
     * Frames") and "Undo Slice Sheet" would describe an edit that never happened.
     */
    struct SheetAutoName
    {
        OpaaxString            SheetPath;
        TDynArray<SpriteFrame> Before;
        TDynArray<SpriteFrame> After;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Auto-Name Frames"; }
    };

    /**
     * ONE frame's rect or name changed — a drag on the canvas, or a field in the panel.
     *
     * The gesture pattern EntityComponentsEdit already uses, and for the same reason: a drag writes
     * through the live data every frame, so the step is bracketed around the gesture rather than
     * recorded per mutation. End() answers false when nothing actually moved, so a click that
     * missed, or a drag that returned home, is not a step.
     */
    struct SheetFrameEdit
    {
        OpaaxString SheetPath;
        Uint32      Index = 0;
        SpriteFrame Before;
        SpriteFrame After;

        /** Cache the frame as the BEFORE half. */
        void Begin(const EditorContext& InContext, Uint32 InIndex);

        /** @return true when the frame differs from what Begin saw. */
        bool End(const EditorContext& InContext);

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Edit Frame"; }
    };

    /** Which frame a sprite with no opinion shows. */
    struct SheetDefaultFrame
    {
        OpaaxString SheetPath;
        Uint32      Before = 0;
        Uint32      After  = 0;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Set Default Frame"; }
    };
}
