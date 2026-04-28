#pragma once
#include "Assets/AssetManifest.h"
#include "Assets/AssetScanner.h"

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxStringID.hpp"
#include "Assets/AssetScanner.h"

namespace Opaax::Editor
{
    // =============================================================================
    // AssetBrowserPanel
    //
    // Displays all assets from the manifest — loaded, unloaded, and missing.
    // Triggers AssetScanner on startup and on manual Refresh.
    //
    // Features:
    //   - Scan on startup
    //   - Refresh button
    //   - Filter by type
    //   - Visual status : loaded (green) / unloaded (white) / missing (red)
    //   - Texture preview on hover
    //   - Drag & drop → SpriteComponent in Inspector
    //
    // Drag & Drop protocol:
    //   Begin : ImGui::SetDragDropPayload("ASSET_ID", &id, sizeof(Uint32))
    //   Accept: InspectorPanel reads "ASSET_ID" payload, calls AssetRegistry::Load<T>
    // =============================================================================
    class OPAAX_API AssetBrowserPanel
    {
    public:
        AssetBrowserPanel()  = default;
        ~AssetBrowserPanel() = default;

        // Called by EditorSubsystem::Startup() — triggers first scan
        void Startup();

        void Draw();

        // Drag & drop payload type — shared with InspectorPanel
        static constexpr const char* DragDropPayloadType = "ASSET_ID";

    private:
        void RunScan();
        void DrawToolbar();
        void DrawAssetList();
        void DrawAssetEntry(const AssetDescriptor& InDesc);
        void DrawTexturePreview(const AssetDescriptor& InDesc);

        // Filter state
        OpaaxString     m_FilterText;
        OpaaxStringID   m_FilterType;   // empty = show all

        // Last scan result — displayed in toolbar
        AssetScanner::ScanResult m_LastScanResult;
        bool m_bScanned = false;

        // Currently hovered asset for preview
        OpaaxStringID m_HoveredID;

        // Cached manifest abs path for saves
        OpaaxString m_ManifestAbsPath;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR