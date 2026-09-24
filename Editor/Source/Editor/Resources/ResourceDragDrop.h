#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    // =============================================================================
    // The drag payload for a resource FILE — ONE payload for every resource type, carrying the type
    //   it is rather than announcing it in the payload's name.
    //
    //   That is what keeps the browser generic: it drags whatever the engine's format table already
    //   told it the file is, with no per-type code, and the DROP side decides whether that type is
    //   the one it wants. A payload id per resource type would have put "is this a texture?" in the
    //   browser, which is precisely the knowledge the format registry exists to hold instead.
    //
    //   Layout: [Uint32 TypeId][path bytes]. Variable length and NOT null-terminated — a fixed
    //   char[MAX_PATH] would either truncate a long path silently or waste the difference, and
    //   ImGui already copies the payload with an explicit size.
    //
    //   The path is ASSET-RELATIVE. The conversion happens at the SOURCE, which has the
    //   EditorContext (and therefore IPaths) to do it with; the drop target is a property drawer,
    //   which by contract has neither.
    // =============================================================================

    /** ImGui payload id. ImGui caps these at 32 bytes, and enforces it with an assert. */
    inline constexpr const char* RESOURCE_PAYLOAD_ID = "OPAAX_RESOURCE";

    /**
     * Hand ImGui a payload for InAssetPath. Call between BeginDragDropSource / EndDragDropSource.
     *
     * @param InTypeId ResourceTypeID of the type claiming this file's extension.
     * @param InAssetPath Asset-relative path, as a component stores it. An EMPTY path sets no
     *   payload — a file outside the project's Assets dir is simply not draggable into a field.
     */
    void SetResourceDragPayload(Uint32 InTypeId, const OpaaxString& InAssetPath);

    /**
     * Accept a dropped resource path IFF it carries InTypeId. Call immediately after the widget
     * that should receive it; it opens and closes the drag-drop target itself.
     *
     * A payload of the WRONG type is left untouched, so ImGui shows no accept highlight over this
     * widget and the drag stays live for a target that does want it — the refusal is visible before
     * the mouse is released, not after.
     *
     * @param OutAssetPath Written only when something was accepted.
     * @return true when OutAssetPath was written this frame.
     */
    bool AcceptResourceDragPayload(Uint32 InTypeId, OpaaxString& OutAssetPath);
}
