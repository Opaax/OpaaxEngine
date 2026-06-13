#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxStringID.hpp"
#include "Assets/AssetScanner.h"
#include "Editor/EditorEventBus.h"
#include "Editor/IEditorPanel.h"
#include "Editor/Panels/AssetBrowserFilter.h"

namespace Opaax
{
    class SceneManager;
    class IEditorUIBackend;
}

namespace Opaax::Editor
{
    // =============================================================================
    // AssetTreeNode
    // =============================================================================
    /**
     * @struct AssetTreeNode
     * One node in the Asset Browser folder tree. A node is a folder (its segment
     * Name) that may hold sub-folders (Children) and/or assets directly (Leaves).
     * The two top-level roots are the virtual "Engine" and project-name folders;
     * everything beneath comes from the on-disk folder structure + each asset's
     * logical ID path. Rebuilt from the manifest + cached folders every frame —
     * Leaves are non-owning views into the manifest.
     */
    struct AssetTreeNode
    {
        OpaaxString                       Name;     // folder segment label ("Textures")
        TDynArray<AssetTreeNode>          Children; // sub-folders, sorted by Name at draw time
        TDynArray<const AssetDescriptor*> Leaves;   // assets directly in this folder (non-owning)
    };

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
    public:
        // Public for the OnSceneSavedEvent subscriber + the manual Refresh toolbar button.
        void RunScan();

        // Primary entry point — called directly by EditorSubsystem (mirrors HierarchyPanel pattern).
        // The SceneManager is needed to mark the currently loaded scene as such in the list; the
        // UI backend resolves per-backend ImGui texture handles for asset thumbnails.
        void Draw(SceneManager& InSceneMgr, IEditorUIBackend& InUIBackend);

    private:
        void DrawToolbar();
        // NOTE: DrawAssetList is the legacy flat view — retained until DrawAssetTree clears the gates, then deleted.
        void DrawAssetList(SceneManager& InSceneMgr, IEditorUIBackend& InUIBackend);
        void DrawAssetTree(SceneManager& InSceneMgr, IEditorUIBackend& InUIBackend);
        void DrawFolderNode(AssetTreeNode& InNode, bool bIsRoot, bool bFiltering,
                            SceneManager& InSceneMgr, IEditorUIBackend& InUIBackend);
        void DrawAssetEntry(const AssetDescriptor& InDesc, const char* InDisplayName,
                            SceneManager& InSceneMgr, IEditorUIBackend& InUIBackend);

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IEditorPanel interface
    public:
        void        Startup()                          override;
        void        OnSubscribe(EditorEventBus& InBus) override;
        const char* GetPanelName() const               override { return "Asset Browser"; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        AssetBrowserFilter       m_Filter;
        AssetScanner::ScanResult m_LastScanResult;
        bool                     m_bScanned = false;

        // On-disk folder structure (root-relative, e.g. "Textures", "Scenes/Sub"), refreshed every
        // RunScan. Seeds the tree skeleton so empty folders show even without any asset inside them.
        TDynArray<OpaaxString>   m_EngineFolders;
        TDynArray<OpaaxString>   m_ProjectFolders;
        OpaaxStringID            m_HoveredID;
        OpaaxString              m_ManifestAbsPath;

        // Set inside DrawAssetEntry's context menu; processed once after the iteration
        // ends, so we never mutate AssetManifest::s_Descriptors while iterating it.
        OpaaxStringID            m_PendingRemoveID;

        SubscriptionToken        m_SceneSavedToken;

        // Cached at OnSubscribe so a single-click can publish OnAssetSelectedEvent to the
        // AssetDetailsPanel. Non-owning — the bus outlives the panel (EditorSubsystem owns both).
        EditorEventBus*          m_Bus = nullptr;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
