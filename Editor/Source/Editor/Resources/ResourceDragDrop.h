#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    // =============================================================================
    // The drag payload for a resource file: one payload for every resource type, carrying the type id.
    //   The browser drags any file without per-type code; the drop target decides if it wants the type.
    //   The bytes and the accept rule are in ResourceDragPayload.h.
    //   The path is asset-relative, converted by the drag source (the drop target has no IPaths).
    // =============================================================================

    /** ImGui payload id (ImGui caps these at 32 bytes). */
    inline constexpr const char* RESOURCE_PAYLOAD_ID = "OPAAX_RESOURCE";

    /**
     * Gives ImGui a payload for InAssetPath. Call between BeginDragDropSource / EndDragDropSource.
     * @param InTypeId ResourceTypeID of the type registered for this file's extension
     * @param InAssetPath Asset-relative path. Empty sets no payload (a file outside the project's
     *   assets cannot be dropped into a field)
     * @param InSubTypeId What kind inside the type (a .opaaxdata's data type id); 0 for none
     */
    void SetResourceDragPayload(Uint32 InTypeId, const OpaaxString& InAssetPath, Uint32 InSubTypeId = 0);

    /**
     * Accepts a dropped resource path if it carries InTypeId. Call right after the receiving widget;
     * it opens and closes the drop target itself. A payload of another type is ignored, so the widget
     * shows no highlight and the drag stays live.
     * @param OutAssetPath Written only when something was accepted
     * @param InSubTypeId Also require this sub-type (e.g. one data type); 0 accepts any
     * @return True when OutAssetPath was written this frame
     */
    bool AcceptResourceDragPayload(Uint32 InTypeId, OpaaxString& OutAssetPath, Uint32 InSubTypeId = 0);
}
