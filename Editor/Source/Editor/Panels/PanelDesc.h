#pragma once

#include "Core/OpaaxTypes.h"               // Uint8
#include "Core/String/OpaaxStringID.hpp"   // OPAAX_ID
#include "Core/Tag/OpaaxTag.h"             // OpaaxTag

namespace Opaax::Editor
{
    /** Whether a panel's window is open when the editor starts. */
    enum class EPanelVisibility : Uint8
    {
        Visible,
        Hidden
    };

    /** Enum to string. */
    inline const char* ToString(const EPanelVisibility InVisibility) noexcept
    {
        return InVisibility == EPanelVisibility::Hidden ? "hidden" : "visible";
    }

    // =============================================================================
    // PanelDesc — what the editor knows about a panel besides the panel itself. No ImGui types
    //   (game modules include this). Id is the registry key, the window label and the dock key.
    // =============================================================================
    struct PanelDesc
    {

        /** Identity, label and dock key. Shown as is, spaces allowed. */
        OpaaxStringID Id;

        /** The root menu that holds this panel's toggle ("Tools" for tools). */
        OpaaxStringID Menu = OPAAX_ID("Panels");

        EPanelVisibility DefaultVisibility = EPanelVisibility::Visible;

        /**
         * The command Ctrl+S runs while this panel is focused. Invalid = the panel does not save
         * (Ctrl+S saves the level).
         */
        OpaaxTag SaveCommand;

        /**
         * The commands Ctrl+Z / Ctrl+Y run while this panel is focused. Invalid = the level's history.
         */
        OpaaxTag UndoCommand;
        OpaaxTag RedoCommand;

        /**
         * The command Delete runs while this panel is focused. Invalid = delete the level selection.
         */
        OpaaxTag DeleteCommand;
    };
}
