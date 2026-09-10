#pragma once

#include "Core/OpaaxTypes.h"               // Uint8
#include "Core/String/OpaaxStringID.hpp"   // OPAAX_ID — the panel's interned identity
#include "Core/Tag/OpaaxTag.h"             // the command Ctrl+S runs while this panel is focused

namespace Opaax::Editor
{
    /** Whether a panel's window is open when the editor starts. */
    enum class EPanelVisibility : Uint8
    {
        Visible,
        Hidden
    };

    /** **I11** — an enum gets a free ToString, found by ADL, declared with the enum. */
    inline const char* ToString(const EPanelVisibility InVisibility) noexcept
    {
        return InVisibility == EPanelVisibility::Hidden ? "hidden" : "visible";
    }

    // =============================================================================
    // PanelDesc — everything the editor knows about a panel that is not the panel itself.
    //
    //   Id is the ONE identity: registry key, ImGui window label, and dock key in imgui.ini. It used
    //   to be stated twice — here and again as a member inside the panel class — with nothing making
    //   the two agree.
    //
    //   Data only, no ImGui type: a game module includes this header.
    // =============================================================================
    struct PanelDesc
    {

        /** Identity, label and dock key. Displayed verbatim, so spaces are fine. */
        OpaaxStringID Id;

        /** The root menu category holding this panel's toggle — "Tools" for a tool-shaped panel. */
        OpaaxStringID Menu = OPAAX_ID("Panels");

        EPanelVisibility DefaultVisibility = EPanelVisibility::Visible;

        /**
         * The command Ctrl+S runs while this panel is focused. Invalid = this panel does not save,
         * and the chord falls through to the map.
         *
         * DECLARED HERE rather than matched in HandleAuthoringShortcuts, which used to hold a
         * hand-written chain of "is the sheet focused? the clip? the library?". That chain was
         * forgotten FOUR times — MoveMode and Mover shipped without it in ⑦-A, and both input
         * panels in ⑦-B — and the failure is silent and expensive: Ctrl+S in a document editor
         * saved the MAP instead, which is exactly the surprise the chain existed to prevent.
         *
         * A panel that owns a document is the only thing that knows what saving it means, so it
         * says so at its registration and nothing central has to be kept in step.
         */
        OpaaxTag SaveCommand;

        /**
         * The commands Ctrl+Z / Ctrl+Y run while this panel is focused. Invalid = the LEVEL's
         * history (**UN1**), which is right for every panel whose edits land on that stack.
         *
         * SaveCommand's rule one chord over, and for its reason: the target follows the focused
         * panel, and a panel DECLARES its answer instead of a ladder elsewhere remembering it
         * ([[L86]]). This was a bool that only SWALLOWED the chord (⑦-C P6 — Ctrl+Z in the prefab
         * editor was undoing the level behind it); a document with a history of its own names the
         * commands that step it (P8 V3).
         */
        OpaaxTag UndoCommand;
        OpaaxTag RedoCommand;
    };
}
