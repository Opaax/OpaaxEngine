#include "AssetBrowserPanel.h"

#if OPAAX_WITH_EDITOR

#include <algorithm>
#include <filesystem>
#include <string_view>
#include <imgui.h>
#include "Assets/AssetRegistry.h"
#include "Assets/AssetManifest.h"
#include "Assets/AssetScanner.h"
#include "Core/OpaaxPath.h"
#include "Core/Log/OpaaxLog.h"
#include "Core/Config/EngineConfig.h"
#include "Core/Config/ProjectConfig.h"
#include "Editor/Assets/AssetTypeRegistry.h"
#include "Editor/Assets/IAssetTypeActions.h"
#include "Editor/Events/EditorEvents.h"
#include "Scene/SceneManager.h"

namespace Opaax::Editor
{
    // Compares an entry's resolved abs path against SceneManager::GetCurrentScenePath()
    // via std::filesystem::equivalent so slash/case differences don't cause false negatives.
    // Returns false for non-Scene entries, missing files, or when no scene is loaded.
    static bool IsLoadedScene(const SceneManager& InMgr, const AssetDescriptor& InDesc)
    {
        if (!InMgr.HasCurrentScenePath())          { return false; }
        if (InDesc.Type != OpaaxStringID("Scene")) { return false; }
        if (InDesc.bMissing)                       { return false; }

        std::error_code lEc;
        const OpaaxString lEntryAbs = OpaaxPath::ToAbsolute(InDesc.RelPath);
        const bool bSame = std::filesystem::equivalent(
            std::filesystem::path(InMgr.GetCurrentScenePath().CStr()),
            std::filesystem::path(lEntryAbs.CStr()),
            lEc);
        return bSame && !lEc;
    }

    // =============================================================================
    // Folder-tree helpers
    // =============================================================================

    // Walk a root directory and collect every sub-folder as a root-relative path
    // ("Textures", "Scenes/Sub") — lets the tree show empty folders, not just ones
    // inferred from asset file paths. Mirrors AssetScanner's exists-then-iterate style.
    static void CollectFolders(const OpaaxString& InRootRel, TDynArray<OpaaxString>& OutFolders)
    {
        const OpaaxString            lAbsRoot = OpaaxPath::ToAbsolute(InRootRel);
        const std::filesystem::path  lRootPath(lAbsRoot.CStr());

        if (!std::filesystem::exists(lRootPath)) { return; }

        for (const auto& lEntry : std::filesystem::recursive_directory_iterator(lRootPath))
        {
            if (!lEntry.is_directory()) { continue; }

            const std::filesystem::path lRel    = std::filesystem::relative(lEntry.path(), lRootPath);
            const std::string           lRelStr = lRel.generic_string();
            if (!lRelStr.empty()) { OutFolders.push_back(OpaaxString(lRelStr.c_str())); }
        }
    }

    // An asset belongs to the Engine root when its (project-root-relative) RelPath
    // starts with the configured engine assets root; everything else is Project.
    static bool IsUnderEngineRoot(const AssetDescriptor& InDesc)
    {
        const OpaaxString&     lEngineRoot = EngineConfig::EngineAssetsRoot();
        const std::string_view lRel(InDesc.RelPath.CStr(), InDesc.RelPath.GetLength());
        const std::string_view lPrefix(lEngineRoot.CStr(), lEngineRoot.GetLength());
        return lRel.starts_with(lPrefix);
    }

    // Split a forward-slash path into its segments (empty segments dropped).
    static void SplitPath(const OpaaxString& InPath, TDynArray<OpaaxString>& OutSegments)
    {
        const Uint32 lLen   = InPath.GetLength();
        Uint32       lStart = 0;
        for (Uint32 i = 0; i < lLen; ++i)
        {
            if (InPath[i] == '/')
            {
                if (i > lStart) { OutSegments.push_back(InPath.SubString(lStart, i - lStart)); }
                lStart = i + 1;
            }
        }
        if (lStart < lLen) { OutSegments.push_back(InPath.SubString(lStart, lLen - lStart)); }
    }

    // Find a child folder by name under InParent, creating it if absent.
    static AssetTreeNode& FindOrAddChild(AssetTreeNode& InParent, const OpaaxString& InName)
    {
        for (auto& lChild : InParent.Children)
        {
            if (lChild.Name == InName) { return lChild; }
        }
        AssetTreeNode lNew;
        lNew.Name = InName;
        InParent.Children.push_back(Move(lNew));
        return InParent.Children.back();
    }

    // Ensure the full folder chain "A/B/C" exists under InRoot.
    static void EnsureFolder(AssetTreeNode& InRoot, const OpaaxString& InFolderPath)
    {
        TDynArray<OpaaxString> lSegments;
        SplitPath(InFolderPath, lSegments);

        AssetTreeNode* lCurrent = &InRoot;
        for (const auto& lSeg : lSegments)
        {
            lCurrent = &FindOrAddChild(*lCurrent, lSeg);
        }
    }

    // Place a descriptor under InRoot using its logical ID as the folder path —
    // all but the last ID segment are folders; the last is the leaf.
    static void InsertLeaf(AssetTreeNode& InRoot, const AssetDescriptor& InDesc)
    {
        TDynArray<OpaaxString> lSegments;
        SplitPath(InDesc.ID.ToString(), lSegments);

        if (lSegments.empty())
        {
            InRoot.Leaves.push_back(&InDesc);
            return;
        }

        AssetTreeNode* lCurrent     = &InRoot;
        const Uint32   lFolderCount = static_cast<Uint32>(lSegments.size()) - 1;
        for (Uint32 i = 0; i < lFolderCount; ++i)
        {
            lCurrent = &FindOrAddChild(*lCurrent, lSegments[i]);
        }
        lCurrent->Leaves.push_back(&InDesc);
    }

    // Recursively sort folders (by Name) and leaves (by ID) for stable display.
    static void SortNode(AssetTreeNode& InNode)
    {
        std::sort(InNode.Children.begin(), InNode.Children.end(),
            [](const AssetTreeNode& A, const AssetTreeNode& B)
            { return std::strcmp(A.Name.CStr(), B.Name.CStr()) < 0; });

        std::sort(InNode.Leaves.begin(), InNode.Leaves.end(),
            [](const AssetDescriptor* A, const AssetDescriptor* B)
            { return std::strcmp(A->ID.ToString().CStr(), B->ID.ToString().CStr()) < 0; });

        for (auto& lChild : InNode.Children) { SortNode(lChild); }
    }

    // Last path segment of a logical ID ("Physics/Ground" → "Ground").
    static OpaaxString LeafName(const OpaaxString& InID)
    {
        const Uint32 lLen   = InID.GetLength();
        Uint32       lLast  = 0;
        bool         bFound = false;
        for (Uint32 i = 0; i < lLen; ++i)
        {
            if (InID[i] == '/') { lLast = i; bFound = true; }
        }
        return bFound ? InID.SubString(lLast + 1) : InID;
    }

    // =============================================================================
    // Startup
    // =============================================================================
    void AssetBrowserPanel::Startup()
    {
        m_ManifestAbsPath = OpaaxPath::ToAbsolute(ProjectConfig::AssetsManifestRelPath());
        RunScan();
    }

    // =============================================================================
    // Subscribe
    // =============================================================================
    void AssetBrowserPanel::OnSubscribe(EditorEventBus& InBus)
    {
        m_Bus = &InBus;

        m_SceneSavedToken = InBus.Subscribe<OnSceneSavedEvent>(
            [this](const OnSceneSavedEvent& InEvent)
            {
                OPAAX_CORE_INFO("AssetBrowserPanel - Scene saved received, rescanning: {}",
                    InEvent.GetPath().CStr());
                RunScan();
            });
    }

    // =============================================================================
    // Scan
    // =============================================================================
    void AssetBrowserPanel::RunScan()
    {
        struct ScanTarget { const char* RootDir; const char* ManifestRelPath; };
        const ScanTarget lScanTargets[] =
        {
            { ProjectConfig::AssetsRoot().CStr(),       ProjectConfig::AssetsManifestRelPath().CStr() },
            { EngineConfig::EngineAssetsRoot().CStr(),  EngineConfig::EngineManifestRelPath().CStr()  },
        };

        for (const auto& lTarget : lScanTargets)
        {
            AssetScanner::ScanConfig lConfig;
            lConfig.RootDir         = lTarget.RootDir;
            lConfig.ManifestAbsPath = OpaaxPath::ToAbsolute(lTarget.ManifestRelPath);
            lConfig.bFlagMissing    = true;

            m_LastScanResult = AssetScanner::Scan(lConfig);

            OPAAX_CORE_INFO("AssetBrowserPanel: scan '{}' — +{} new, {} existing, {} missing",
                lTarget.RootDir,
                m_LastScanResult.Added,
                m_LastScanResult.Existing,
                m_LastScanResult.Missing);
        }

        // Refresh the on-disk folder cache so the tree shows folders (including empty ones),
        // not only folders inferred from asset file paths.
        m_EngineFolders.clear();
        m_ProjectFolders.clear();
        CollectFolders(EngineConfig::EngineAssetsRoot(), m_EngineFolders);
        CollectFolders(ProjectConfig::AssetsRoot(),      m_ProjectFolders);

        m_bScanned = true;
    }

    // =============================================================================
    // Draw
    // =============================================================================
    void AssetBrowserPanel::Draw(SceneManager& InSceneMgr, IEditorUIBackend& InUIBackend)
    {
        ImGui::Begin("Asset Browser");
        DrawToolbar();
        ImGui::Separator();
        DrawAssetTree(InSceneMgr, InUIBackend);
        ImGui::End();

        // Deferred manifest mutation — safe to mutate s_Descriptors here, after iteration ended.
        if (m_PendingRemoveID.IsValid())
        {
            const OpaaxStringID lID = m_PendingRemoveID;
            m_PendingRemoveID = OpaaxStringID{};

            if (AssetRegistry::IsLoaded(lID)) { AssetRegistry::Unload(lID); }
            AssetManifest::Remove(lID);
            RunScan(); // persists the now-shorter manifest via SaveManifest's prefix filter
        }
    }

    // =============================================================================
    // Toolbar
    // =============================================================================
    void AssetBrowserPanel::DrawToolbar()
    {
        if (ImGui::Button("  Refresh  "))
        {
            RunScan();
        }

        if (m_bScanned)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("+%u  |  %u  |  ",
                m_LastScanResult.Added,
                m_LastScanResult.Existing);

            if (m_LastScanResult.Missing > 0)
            {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.3f, 0.3f, 1.f));
                ImGui::Text("%u missing", m_LastScanResult.Missing);
                ImGui::PopStyleColor();
            }
        }

        ImGui::Spacing();

        // Text filter
        {
            char lBuf[128];
            strncpy_s(lBuf, sizeof(lBuf), m_Filter.Text.CStr(), _TRUNCATE);
            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::InputTextWithHint("##Filter", "Filter assets...", lBuf, sizeof(lBuf)))
            {
                m_Filter.Text = OpaaxString(lBuf);
            }
        }

        ImGui::Spacing();

        // Type filter — "All" button, then one per registered asset type
        {
            const bool bAllSelected = !m_Filter.TypeID.IsValid();
            
            if (bAllSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.f));
            }
            
            if (ImGui::SmallButton("All"))
            {
                m_Filter.TypeID = OpaaxStringID{};
            }
            
            if (bAllSelected)
            {
                ImGui::PopStyleColor();
            }
        }

        for (const auto& lActions : AssetTypeRegistry::GetAll())
        {
            ImGui::SameLine();
            
            const bool bSelected = (m_Filter.TypeID == lActions->GetTypeID());
            
            if (bSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.f));
            }
            
            if (ImGui::SmallButton(lActions->GetLabel()))
            {
                m_Filter.TypeID = lActions->GetTypeID();
            }
            
            if (bSelected)
            {
                ImGui::PopStyleColor();
            }
        }
    }

    // =============================================================================
    // Asset list
    // =============================================================================
    void AssetBrowserPanel::DrawAssetList(SceneManager& InSceneMgr, IEditorUIBackend& InUIBackend)
    {
        const auto& lAll = AssetManifest::GetAll();

        if (lAll.empty())
        {
            ImGui::TextDisabled("No assets in manifest. Click Refresh to scan.");
            return;
        }

        Int32 lVisible = 0;

        for (const auto& [lKey, lDesc] : lAll)
        {
            if (!m_Filter.Matches(lDesc)) { continue; }
            const OpaaxString lName = lDesc.ID.ToString();
            DrawAssetEntry(lDesc, lName.CStr(), InSceneMgr, InUIBackend);
            ++lVisible;
        }

        if (lVisible == 0)
        {
            ImGui::TextDisabled("No assets match the current filter.");
        }
    }

    // =============================================================================
    // Asset tree — Engine / <Project> roots, on-disk folders, logical-ID leaves
    // =============================================================================
    void AssetBrowserPanel::DrawAssetTree(SceneManager& InSceneMgr, IEditorUIBackend& InUIBackend)
    {
        const auto& lAll = AssetManifest::GetAll();

        // Roots: "Engine" is fixed; the project root uses the active project's name (e.g. "Game").
        AssetTreeNode lEngineRoot;  lEngineRoot.Name  = OpaaxString("Engine");
        AssetTreeNode lProjectRoot; lProjectRoot.Name = ProjectConfig::Name();

        const bool bFiltering = !m_Filter.Text.IsEmpty() || m_Filter.TypeID.IsValid();

        // Unfiltered: seed the full on-disk folder skeleton so empty folders show too.
        // Filtered: skip it, so only folders containing a match appear (and auto-expand).
        if (!bFiltering)
        {
            for (const auto& lFolder : m_EngineFolders)  { EnsureFolder(lEngineRoot,  lFolder); }
            for (const auto& lFolder : m_ProjectFolders) { EnsureFolder(lProjectRoot, lFolder); }
        }

        Int32 lVisible = 0;
        for (const auto& [lKey, lDesc] : lAll)
        {
            if (!m_Filter.Matches(lDesc)) { continue; }
            InsertLeaf(IsUnderEngineRoot(lDesc) ? lEngineRoot : lProjectRoot, lDesc);
            ++lVisible;
        }

        const bool bEmpty = lEngineRoot.Children.empty()  && lEngineRoot.Leaves.empty()
                         && lProjectRoot.Children.empty() && lProjectRoot.Leaves.empty();
        if (bEmpty)
        {
            ImGui::TextDisabled(bFiltering
                ? "No assets match the current filter."
                : "No assets in manifest. Click Refresh to scan.");
            return;
        }

        SortNode(lEngineRoot);
        SortNode(lProjectRoot);

        DrawFolderNode(lEngineRoot,  /*bIsRoot*/true, bFiltering, InSceneMgr, InUIBackend);
        DrawFolderNode(lProjectRoot, /*bIsRoot*/true, bFiltering, InSceneMgr, InUIBackend);
    }

    // =============================================================================
    // Folder node — one tree level; recurses into sub-folders then draws leaves
    // =============================================================================
    void AssetBrowserPanel::DrawFolderNode(AssetTreeNode& InNode, bool bIsRoot, bool bFiltering,
                                           SceneManager& InSceneMgr, IEditorUIBackend& InUIBackend)
    {
        // A root with nothing under it (e.g. project has no folders/assets yet) is skipped;
        // empty *sub*-folders are kept on purpose so the on-disk structure is visible.
        if (bIsRoot && InNode.Children.empty() && InNode.Leaves.empty()) { return; }

        ImGuiTreeNodeFlags lFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (bIsRoot) { lFlags |= ImGuiTreeNodeFlags_DefaultOpen; }

        // Active filter → force-open so matches are visible without manual expansion.
        if (bFiltering) { ImGui::SetNextItemOpen(true, ImGuiCond_Always); }

        const bool bOpen = ImGui::TreeNodeEx(InNode.Name.CStr(), lFlags);
        if (bOpen)
        {
            for (auto& lChild : InNode.Children)
            {
                DrawFolderNode(lChild, /*bIsRoot*/false, bFiltering, InSceneMgr, InUIBackend);
            }
            for (const AssetDescriptor* lLeaf : InNode.Leaves)
            {
                const OpaaxString lShortName = LeafName(lLeaf->ID.ToString());
                DrawAssetEntry(*lLeaf, lShortName.CStr(), InSceneMgr, InUIBackend);
            }
            ImGui::TreePop();
        }
    }

    // =============================================================================
    // Single entry — icon, colour, D&D source, tooltip, context menu
    // =============================================================================
    void AssetBrowserPanel::DrawAssetEntry(const AssetDescriptor& InDesc, const char* InDisplayName,
                                           SceneManager& InSceneMgr, IEditorUIBackend& InUIBackend)
    {
        // Scenes don't go through AssetRegistry — they're owned by SceneManager.
        // Treat the entry that matches GetCurrentScenePath() as "loaded" for display parity with textures.
        const bool bLoaded  = AssetRegistry::IsLoaded(InDesc.ID)
                           || IsLoadedScene(InSceneMgr, InDesc);
        const bool bMissing = InDesc.bMissing;

        // Status colour
        ImVec4 lColor;
        if (bMissing)       { lColor = ImVec4(1.f,  0.3f, 0.3f, 1.f); }
        else if (bLoaded)   { lColor = ImVec4(0.4f, 1.f,  0.4f, 1.f); }
        else                { lColor = ImVec4(1.f,  1.f,  1.f,  1.f); }

        ImGui::PushStyleColor(ImGuiCol_Text, lColor);

        const char*       lIcon  = AssetTypeRegistry::GetIcon(InDesc.Type);
        const OpaaxString lIDStr = InDesc.ID.ToString();

        // Label shows the short display name; the ##id keeps it globally unique across folders.
        char lLabel[256];
        snprintf(lLabel, sizeof(lLabel), "%s  %s##%u", lIcon, InDisplayName, InDesc.ID.GetId());

        const bool bClicked = ImGui::Selectable(lLabel, m_HoveredID == InDesc.ID);
        ImGui::PopStyleColor();

        // Single-click selects → AssetDetailsPanel renders this asset's editor/preview.
        if (bClicked && m_Bus)
        {
            m_Bus->Publish(OnAssetSelectedEvent{ InDesc.ID, InDesc.Type });
            OPAAX_CORE_INFO("AssetBrowserPanel - selected '{}' ({})",
                InDesc.ID.ToString().CStr(), InDesc.Type.ToString().CStr());
        }

        // --- Drag & Drop source ---
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            const Uint32 lID = InDesc.ID.GetId();
            ImGui::SetDragDropPayload(IAssetTypeActions::DragDropPayloadType, &lID, sizeof(Uint32));
            ImGui::Text("  %s  %s", lIcon, lIDStr.CStr());

            IAssetTypeActions* lActions = AssetTypeRegistry::Find(InDesc.Type);
            if (lActions && lActions->CanPreview() && bLoaded)
            {
                lActions->DrawPreview(InDesc.ID, InUIBackend);
            }

            ImGui::EndDragDropSource();
        }

        // --- Hover tooltip ---
        if (ImGui::IsItemHovered())
        {
            m_HoveredID = InDesc.ID;

            ImGui::BeginTooltip();
            ImGui::TextDisabled("ID   : %s", lIDStr.CStr());
            ImGui::TextDisabled("Path : %s", InDesc.RelPath.CStr());
            ImGui::TextDisabled("Type : %s", InDesc.Type.ToString().CStr());
            ImGui::TextDisabled("Status : %s",
                bMissing ? "MISSING" : bLoaded ? "Loaded" : "Not loaded");

            IAssetTypeActions* lActions = AssetTypeRegistry::Find(InDesc.Type);
            if (lActions && lActions->CanPreview() && bLoaded)
            {
                ImGui::Spacing();
                lActions->DrawPreview(InDesc.ID, InUIBackend);
            }

            ImGui::EndTooltip();
        }
        else if (m_HoveredID == InDesc.ID)
        {
            m_HoveredID = OpaaxStringID{};
        }

        // --- Right-click context menu ---
        if (ImGui::BeginPopupContextItem())
        {
            IAssetTypeActions* lActions = AssetTypeRegistry::Find(InDesc.Type);

            if (!bMissing && !bLoaded && lActions)
            {
                if (ImGui::MenuItem("Load")) { lActions->Load(InDesc.ID); }
            }

            if (bLoaded && lActions)
            {
                if (ImGui::MenuItem("Reload")) { lActions->Reload(InDesc.ID); }
                if (ImGui::MenuItem("Unload")) { AssetRegistry::Unload(InDesc.ID); }
            }

            if (bMissing)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.4f, 0.4f, 1.f));
                if (ImGui::MenuItem("Remove from manifest"))
                {
                    m_PendingRemoveID = InDesc.ID;
                }
                ImGui::PopStyleColor();
            }

            ImGui::Separator();
            ImGui::TextDisabled("%s", InDesc.RelPath.CStr());
            ImGui::EndPopup();
        }

        // --- Double-click: invoke the type's primary action via IAssetTypeActions ---
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        {
            if (IAssetTypeActions* lActions = AssetTypeRegistry::Find(InDesc.Type))
            {
                lActions->Load(InDesc.ID);
            }
        }
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
