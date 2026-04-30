#pragma once

#if OPAAX_WITH_EDITOR

#include "Editor/Assets/IAssetTypeActions.h"

namespace Opaax::Editor
{
    /**
     * @class Texture2DTypeActions
     * IAssetTypeActions for Texture2D assets.
     * Centralises the OpenGLTexture2D coupling — load, reload, and ImGui thumbnail preview.
     */
    class OPAAX_API Texture2DTypeActions final : public IAssetTypeActions
    {
    public:
        OpaaxStringID GetTypeID() const override { return OpaaxStringID("Texture2D"); }
        const char*   GetIcon()   const override { return "[ T ]"; }
        const char*   GetLabel()  const override { return "Texture"; }

        void Load  (OpaaxStringID InID) override;
        void Reload(OpaaxStringID InID) override;

        bool CanPreview()                   const override { return true; }
        void DrawPreview(OpaaxStringID InID)      override;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
