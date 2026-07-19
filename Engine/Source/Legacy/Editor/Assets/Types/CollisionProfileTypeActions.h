#pragma once

#if OPAAX_WITH_EDITOR

#include "Editor/Assets/IAssetTypeActions.h"

namespace Opaax::Editor
{
    // =============================================================================
    // CollisionProfileTypeActions
    //
    // IAssetTypeActions for CollisionProfile. DrawPreview is the read-only hover summary
    // (channel + response matrix as text); DrawEditor is the interactive author surface
    // (channel combo + per-channel response table + Save) shown in the AssetDetailsPanel.
    // =============================================================================
    class OPAAX_API CollisionProfileTypeActions final : public IAssetTypeActions
    {
    public:
        OpaaxStringID GetTypeID() const override { return OpaaxStringID("CollisionProfile"); }
        const char*   GetIcon()   const override { return "[CP]"; }
        const char*   GetLabel()  const override { return "Collision Profile"; }

        void Load  (OpaaxStringID InID) override;
        void Reload(OpaaxStringID InID) override;

        bool CanPreview()                                                 const override { return true; }
        void DrawPreview(OpaaxStringID InID, IEditorUIBackend& InUIBackend)      override;

        bool CanEdit()                                                    const override { return true; }
        void DrawEditor(OpaaxStringID InID, IEditorUIBackend& InUIBackend)       override;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
