#pragma once

#include "Core/OpaaxTypes.h"               // Uint8
#include "Core/String/OpaaxStringID.hpp"   // OPAAX_ID — the panel's interned identity

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
    };
}
