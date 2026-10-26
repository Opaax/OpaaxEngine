#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Resources/ResourceTypeID.hpp"

namespace Opaax
{
    struct AssetResource;
}

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
     * @param InKinds What the file holds, for a type that holds many: an asset's class name id, then its
     *   parents'. Empty for none
     */
    void SetResourceDragPayload(Uint32 InTypeId, const OpaaxString& InAssetPath, const TDynArray<Uint32>& InKinds = {});

    /**
     * Accepts a dropped resource path if it carries InTypeId. Call right after the receiving widget;
     * it opens and closes the drop target itself. A payload of another type is ignored, so the widget
     * shows no highlight and the drag stays live.
     * @param OutAssetPath Written only when something was accepted
     * @param InKind Also require this kind among the file's (an asset class: its subclasses match too);
     *   0 accepts any
     * @return True when OutAssetPath was written this frame
     */
    bool AcceptResourceDragPayload(Uint32 InTypeId, OpaaxString& OutAssetPath, Uint32 InKind = 0);

    /**
     * Accepts a dropped asset file that holds a T or a subclass of T: an .opaaxasset, or a file T reads
     * (a .png for a Texture2D). As AcceptResourceDragPayload.
     */
    template<typename T>
    bool AcceptAssetDragPayload(OpaaxString& OutAssetPath)
    {
        return AcceptResourceDragPayload(ResourceTypeID::Get<AssetResource>(), OutAssetPath,
                                         T::StaticClass().GetName().GetId());
    }
}
