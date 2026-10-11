// SpriteSheetUndoables.h
#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Renderer/Textures/SpriteSheetData.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The undo steps recorded by the sheet editor's actions. Plain structs with Undo / Redo / Label.
    // Every step carries the sheet's path: undoing after opening another sheet does nothing and warns,
    // instead of writing into the wrong file.
    // =============================================================================

    /** The whole frame list was replaced (one slice, however many frames). */
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
     * Every unnamed frame got a name, as one step. Its own type so the Edit menu names it correctly.
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
     * One frame's rect or name changed (a canvas drag or a panel field). Bracketed around the
     * gesture; End() returns false when nothing moved.
     */
    struct SheetFrameEdit
    {
        OpaaxString SheetPath;
        Uint32      Index = 0;
        SpriteFrame Before;
        SpriteFrame After;

        /** Stores the frame as the before state. */
        void Begin(const EditorContext& InContext, Uint32 InIndex);

        /** @return True when the frame differs from what Begin saw. */
        bool End(const EditorContext& InContext);

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Edit Frame"; }
    };

    /** The frame a sprite shows when none is named. */
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
