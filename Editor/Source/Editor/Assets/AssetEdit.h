#pragma once

#include <nlohmann/json.hpp>

#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // AssetEdit — one change to an open asset: its whole values before and after. Kept in that asset's
    //   own history; a step whose asset is no longer open does nothing (with a warning).
    // =============================================================================
    struct AssetEdit
    {
        /** Absolute: how open assets are found. */
        OpaaxString    AssetPath;
        nlohmann::json Before;
        nlohmann::json After;
        OpaaxString    LabelText = OpaaxString("Edit Asset");

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return LabelText.CStr(); }
    };
}
