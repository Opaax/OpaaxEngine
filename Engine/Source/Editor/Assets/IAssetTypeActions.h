#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/OpaaxStringID.hpp"

namespace Opaax
{
    class IEditorUIBackend;
}

namespace Opaax::Editor
{
    /**
     * @class IAssetTypeActions
     * Per-type asset behaviour for the editor: icon, label, load, reload, and preview.
     * Register implementations via AssetTypeRegistry::Register() at startup.
     *
     * DragDropPayloadType — shared payload tag for asset drag & drop between panels.
     * Both AssetBrowserPanel (source) and component drawers (target) reference this constant.
     */
    class OPAAX_API IAssetTypeActions
    {
    public:
        static constexpr const char* DragDropPayloadType = "ASSET_ID";

        virtual ~IAssetTypeActions() = default;

        virtual OpaaxStringID GetTypeID() const = 0;
        virtual const char*   GetIcon()   const = 0;
        virtual const char*   GetLabel()  const = 0;

        virtual void Load  (OpaaxStringID InID) = 0;
        virtual void Reload(OpaaxStringID InID) = 0;

        // Read-only thumbnail/info — rendered in the AssetBrowser hover tooltip + drag source.
        // Those surfaces are non-interactive overlays, so DrawPreview must NEVER place widgets
        // that expect input (combos, buttons). For editing, see CanEdit/DrawEditor below.
        virtual bool CanPreview()                                                 const { return false; }
        virtual void DrawPreview(OpaaxStringID InID, IEditorUIBackend& InUIBackend)      {}

        // Interactive editor — rendered in the persistent AssetDetailsPanel (a real window,
        // so widgets accept input). Override CanEdit()->true and implement DrawEditor to make
        // an asset author-able in the editor.
        virtual bool CanEdit()                                                    const { return false; }
        virtual void DrawEditor(OpaaxStringID InID, IEditorUIBackend& InUIBackend)       {}
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
