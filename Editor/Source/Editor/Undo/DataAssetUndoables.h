#pragma once

#include <nlohmann/json.hpp>

#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // DataAssetEdit — one edit gesture on the open .opaaxdata, as the whole value before and after.
    //   A step whose asset is no longer the open one does nothing (with a warning).
    // =============================================================================
    struct DataAssetEdit
    {
        OpaaxString    AssetPath;
        nlohmann::json Before;
        nlohmann::json After;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Edit Data Asset"; }
    };
}
