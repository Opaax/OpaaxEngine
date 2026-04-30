#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxStringID.hpp"
#include "Assets/AssetScanner.h"
#include "Editor/IEditorPanel.h"
#include "Editor/Panels/AssetBrowserFilter.h"

namespace Opaax::Editor
{
    /**
     * @class AssetBrowserPanel
     * Displays all assets from the manifest — loaded, unloaded, and missing.
     * Triggers AssetScanner on Startup and on manual Refresh.
     *
     * Extension: register an IAssetTypeActions via AssetTypeRegistry::Register()
     * to add icon, load/reload, and preview for a new asset type.
     * This panel adapts automatically — no changes required here.
     *
     * Drag & Drop protocol:
     *   Begin : SetDragDropPayload(IAssetTypeActions::DragDropPayloadType, &id, sizeof(Uint32))
     *   Accept: component drawers call AcceptDragDropPayload with the same tag
     */
    class OPAAX_API AssetBrowserPanel final : public IEditorPanel
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        AssetBrowserPanel()           = default;
        ~AssetBrowserPanel() override = default;
        
        // =============================================================================
        // Functions
        // =============================================================================
    private:
        void RunScan();
        void DrawToolbar();
        void DrawAssetList();
        void DrawAssetEntry(const AssetDescriptor& InDesc);
        
        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IEditorPanel interface
    public:
        void        Startup()            override;
        void        Draw()               override;
        const char* GetPanelName() const override { return "Asset Browser"; }
        //~End IEditorPanel interface
        
        // =============================================================================
        // Members
        // =============================================================================
    private:
        AssetBrowserFilter       m_Filter;
        AssetScanner::ScanResult m_LastScanResult;
        bool                     m_bScanned = false;
        OpaaxStringID            m_HoveredID;
        OpaaxString              m_ManifestAbsPath;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
