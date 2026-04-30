#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/OpaaxStringID.hpp"

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

        virtual bool CanPreview()                    const { return false; }
        virtual void DrawPreview(OpaaxStringID InID)       {}
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
