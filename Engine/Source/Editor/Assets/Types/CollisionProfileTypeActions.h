#pragma once

#if OPAAX_WITH_EDITOR

#include "Editor/Assets/IAssetTypeActions.h"

namespace Opaax::Editor
{
    // =============================================================================
    // CollisionProfileTypeActions
    //
    // IAssetTypeActions for CollisionProfile. The preview is an interactive editor:
    // an object-channel combo + a per-channel response matrix + a Save button that
    // writes the in-memory state back to the asset's source JSON.
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
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
