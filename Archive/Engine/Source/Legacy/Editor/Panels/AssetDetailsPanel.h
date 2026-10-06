#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/OpaaxStringID.hpp"
#include "Editor/EditorEventBus.h"
#include "Editor/IEditorPanel.h"

namespace Opaax
{
    class IEditorUIBackend;
}

namespace Opaax::Editor
{
    /**
     * @class AssetDetailsPanel
     * Persistent, interactive detail view for the asset selected in the AssetBrowser.
     * Subscribes to OnAssetSelectedEvent and dispatches to the asset type's
     * IAssetTypeActions: DrawEditor when CanEdit (this IS a real window, so widgets
     * accept input — unlike the browser's hover preview), else DrawPreview, else basic info.
     */
    class OPAAX_API AssetDetailsPanel final : public IEditorPanel
    {
    public:
        AssetDetailsPanel()           = default;
        ~AssetDetailsPanel() override = default;

        // Draws the panel for the current selection. UI backend forwarded for thumbnails.
        void Draw(IEditorUIBackend& InUIBackend);

        //~Begin IEditorPanel interface
    public:
        void        OnSubscribe(EditorEventBus& InBus) override;
        const char* GetPanelName() const               override { return "Asset Details"; }
        //~End IEditorPanel interface

    private:
        OpaaxStringID     m_SelectedID;
        OpaaxStringID     m_SelectedType;
        SubscriptionToken m_AssetSelectedToken;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
