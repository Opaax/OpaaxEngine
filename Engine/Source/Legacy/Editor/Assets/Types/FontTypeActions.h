#pragma once

#if OPAAX_WITH_EDITOR

#include "Editor/Assets/IAssetTypeActions.h"

namespace Opaax::Editor
{
    // =============================================================================
    // FontTypeActions
    //
    // IAssetTypeActions for FontAsset. Atlas preview + load/reload hooks for the
    // AssetBrowser. Mirrors Texture2DTypeActions shape.
    // =============================================================================
    /**
     * @class FontTypeActions
     * Centralises the FontAsset editor coupling — load, reload, atlas thumbnail.
     */
    class OPAAX_API FontTypeActions final : public IAssetTypeActions
    {
    public:
        OpaaxStringID GetTypeID() const override { return OpaaxStringID("Font"); }
        const char*   GetIcon()   const override { return "[ F ]"; }
        const char*   GetLabel()  const override { return "Font"; }

        void Load  (OpaaxStringID InID) override;
        void Reload(OpaaxStringID InID) override;

        bool CanPreview()                                                 const override { return true; }
        void DrawPreview(OpaaxStringID InID, IEditorUIBackend& InUIBackend)      override;

        bool GetThumbnail(OpaaxStringID InID, IEditorUIBackend& InUIBackend, AssetThumbnail& OutThumb) const override;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
