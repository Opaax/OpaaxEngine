#include "AssetBrowserPanel.h"

#if OPAAX_WITH_EDITOR

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <imgui.h>
#include <nlohmann/json.hpp>
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
#include "Editor/UI/IEditorUIBackend.h"
#include "Renderer/Texture2D.h"
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

    // Direct child folder by name, or nullptr.
    static AssetTreeNode* FindChildByName(AssetTreeNode& InParent, const OpaaxString& InName)
    {
        for (auto& lChild : InParent.Children)
        {
            if (lChild.Name == InName) { return &lChild; }
        }
        return nullptr;
    }

    // =============================================================================
    // Grid-tile drawing helpers (window draw list)
    // =============================================================================
    static constexpr float k_TileSize           = 84.f;
    static constexpr ImU32 k_DefaultFolderColor  = IM_COL32(232, 196, 104, 255); // gold, when no color set

    // Scale a packed color's RGB by InFactor (alpha preserved) — used for the folder's darker tab.
    static ImU32 DarkenColor(ImU32 InColor, float InFactor)
    {
        const int lR = static_cast<int>(((InColor >> IM_COL32_R_SHIFT) & 0xFF) * InFactor);
        const int lG = static_cast<int>(((InColor >> IM_COL32_G_SHIFT) & 0xFF) * InFactor);
        const int lB = static_cast<int>(((InColor >> IM_COL32_B_SHIFT) & 0xFF) * InFactor);
        const int lA =  (InColor >> IM_COL32_A_SHIFT) & 0xFF;
        return IM_COL32(lR, lG, lB, lA);
    }

    // Folder glyph filling [InMin,InMax], tinted by InTint: the PNG override when InIconTex != 0
    // (white art × tint), else a vector folder (body = tint, tab = darker shade).
    static void DrawFolderGlyph(ImDrawList* InDL, ImVec2 InMin, ImVec2 InMax, bool bHovered, Uint64 InIconTex, ImU32 InTint)
    {
        if (bHovered)
        {
            InDL->AddRectFilled(InMin, InMax, ImGui::GetColorU32(ImGuiCol_HeaderHovered), 4.f);
        }

        if (InIconTex != 0)
        {
            const float  lInset = (InMax.x - InMin.x) * 0.10f;
            const ImVec2 lA(InMin.x + lInset, InMin.y + lInset);
            const ImVec2 lB(InMax.x - lInset, InMax.y - lInset);
            // Texture buffers are bottom-up (stb flip) → V-swap; InTint multiplies the (white) art.
            InDL->AddImage(static_cast<ImTextureID>(InIconTex), lA, lB, ImVec2(0.f, 1.f), ImVec2(1.f, 0.f), InTint);
            return;
        }

        //Tab draw
        const float  lW = InMax.x - InMin.x, lH = InMax.y - InMin.y;
        const float  lMx = lW * 0.16f, lMy = lH * 0.22f;
        const ImVec2 lA(InMin.x + lMx, InMin.y + lMy);
        const ImVec2 lB(InMax.x - lMx, InMax.y - lMy);
        const float  lBodyW = lB.x - lA.x, lBodyH = lB.y - lA.y;
        const ImU32  lBody = InTint;
        const ImU32  lTab  = DarkenColor(InTint, 0.84f);
        const float  lR = 3.f;
        const float  lTabH = lBodyH * 0.26f;
        const float  lTabW = lBodyW * 0.30f;
        InDL->AddRectFilled(ImVec2(lA.x, lA.y), ImVec2(lA.x + lTabW, lA.y + lTabH + lR), lTab, lR, ImDrawFlags_RoundCornersTop);
        InDL->AddRectFilled(ImVec2(lA.x, lA.y + lTabH), ImVec2(lB.x, lB.y), lBody, lR);
    }

    // Join path segments with '/' ("Engine", "Textures" → "Engine/Textures").
    static OpaaxString JoinPath(const TDynArray<OpaaxString>& InSegments)
    {
        OpaaxString lResult;
        for (Uint32 i = 0; i < static_cast<Uint32>(InSegments.size()); ++i)
        {
            if (i > 0) { lResult += "/"; }
            lResult += InSegments[i];
        }
        return lResult;
    }

    // Generic asset card filling [InMin,InMax] with the type-icon text centered + status tint.
    static void DrawAssetGlyph(ImDrawList* InDL, ImVec2 InMin, ImVec2 InMax,
                               const char* InTypeIcon, bool bMissing, bool bLoaded, bool bHovered)
    {
        if (bHovered)
        {
            InDL->AddRectFilled(InMin, InMax, ImGui::GetColorU32(ImGuiCol_HeaderHovered), 4.f);
        }

        const float  lW = InMax.x - InMin.x, lH = InMax.y - InMin.y;
        const float  lMx = lW * 0.16f, lMy = lH * 0.16f;
        const ImVec2 lA(InMin.x + lMx, InMin.y + lMy);
        const ImVec2 lB(InMax.x - lMx, InMax.y - lMy);
        const ImU32  lCard = bMissing ? IM_COL32(120, 60, 60, 255)
                           : bLoaded  ? IM_COL32(70, 90, 70, 255)
                                      : IM_COL32(80, 80, 92, 255);
        InDL->AddRectFilled(lA, lB, lCard, 4.f);
        InDL->AddRect(lA, lB, IM_COL32(0, 0, 0, 120), 4.f);

        const ImVec2 lTs = ImGui::CalcTextSize(InTypeIcon);
        const ImVec2 lAt((lA.x + lB.x) * 0.5f - lTs.x * 0.5f, (lA.y + lB.y) * 0.5f - lTs.y * 0.5f);
        InDL->AddText(lAt, IM_COL32(232, 232, 232, 255), InTypeIcon);
    }

    // One-line label centered under a tile; truncated with an ellipsis when it overflows InWidth.
    static void DrawTileLabel(const char* InText, float InWidth)
    {
        const ImVec2 lFull = ImGui::CalcTextSize(InText);
        if (lFull.x <= InWidth)
        {
            const float lOff = (InWidth - lFull.x) * 0.5f;
            if (lOff > 0.f) { ImGui::SetCursorPosX(ImGui::GetCursorPosX() + lOff); }
            ImGui::TextUnformatted(InText);
            return;
        }

        char         lBuf[160];
        const float  lDotsW = ImGui::CalcTextSize("..").x;
        const size_t lMax   = strlen(InText);
        size_t       lLen   = 0;
        float        lW     = 0.f;
        for (; lLen < lMax && lLen < sizeof(lBuf) - 3; ++lLen)
        {
            const char lC[2] = { InText[lLen], '\0' };
            lW += ImGui::CalcTextSize(lC).x;
            if (lW + lDotsW > InWidth) { break; }
        }
        memcpy(lBuf, InText, lLen);
        lBuf[lLen]     = '.';
        lBuf[lLen + 1] = '.';
        lBuf[lLen + 2] = '\0';
        ImGui::TextUnformatted(lBuf);
    }

    // =============================================================================
    // Startup
    // =============================================================================
    void AssetBrowserPanel::Startup()
    {
        m_ManifestAbsPath = OpaaxPath::ToAbsolute(ProjectConfig::AssetsManifestRelPath());
        RunScan();
        LoadFolderColors();
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

        // Re-attempt the optional PNG folder icon (a freshly-dropped Textures/Editor/Folder.png appears on Refresh).
        m_FolderIconTried = false;

        m_bScanned = true;
    }

    // =============================================================================
    // Draw
    // =============================================================================
    void AssetBrowserPanel::Draw(SceneManager& InSceneMgr, IEditorUIBackend& InUIBackend)
    {
        ImGui::Begin("Asset Browser");

        // Frozen header: the toolbar (+ breadcrumb in grid mode) stay pinned; only the asset
        // area below scrolls, via its own child region (Excel "freeze panes").
        DrawToolbar();
        if (m_View == EBrowserView::Grid) { DrawBreadcrumb(); }
        ImGui::Separator();

        ImGui::BeginChild("##AssetScroll", ImVec2(0.f, 0.f), false);
        switch (m_View)
        {
            case EBrowserView::Grid: DrawAssetGrid(InSceneMgr, InUIBackend); break;
            case EBrowserView::Tree: DrawAssetTree(InSceneMgr, InUIBackend); break;
            case EBrowserView::List: DrawAssetList(InSceneMgr, InUIBackend); break;
        }
        ImGui::EndChild();

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

        // View toggle: Grid (explorer) | Tree (dropdown) | List (flat). Active one is tinted, mirroring the type-filter buttons.
        ImGui::SameLine();
        {
            const auto lViewButton = [this](const char* InLabel, EBrowserView InView)
            {
                const bool bActive = (m_View == InView);
                if (bActive) { ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.f)); }
                if (ImGui::SmallButton(InLabel)) { m_View = InView; }
                if (bActive) { ImGui::PopStyleColor(); }
            };

            lViewButton("Grid", EBrowserView::Grid); ImGui::SameLine();
            lViewButton("Tree", EBrowserView::Tree); ImGui::SameLine();
            lViewButton("List", EBrowserView::List);
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
    // BuildRoots — fill the Engine / <Project> roots from cached folders + manifest
    // =============================================================================
    void AssetBrowserPanel::BuildRoots(AssetTreeNode& OutEngine, AssetTreeNode& OutProject,
                                       bool bSeedSkeleton, bool bFilterLeaves)
    {
        // "Engine" is fixed; the project root uses the active project's name (e.g. "Game").
        OutEngine.Name  = OpaaxString("Engine");
        OutProject.Name = ProjectConfig::Name();

        // Seed the full on-disk folder skeleton so empty folders show too.
        if (bSeedSkeleton)
        {
            for (const auto& lFolder : m_EngineFolders)  { EnsureFolder(OutEngine,  lFolder); }
            for (const auto& lFolder : m_ProjectFolders) { EnsureFolder(OutProject, lFolder); }
        }

        for (const auto& [lKey, lDesc] : AssetManifest::GetAll())
        {
            if (bFilterLeaves && !m_Filter.Matches(lDesc)) { continue; }
            InsertLeaf(IsUnderEngineRoot(lDesc) ? OutEngine : OutProject, lDesc);
        }

        SortNode(OutEngine);
        SortNode(OutProject);
    }

    // =============================================================================
    // Asset tree — Engine / <Project> roots, on-disk folders, logical-ID leaves
    // =============================================================================
    void AssetBrowserPanel::DrawAssetTree(SceneManager& InSceneMgr, IEditorUIBackend& InUIBackend)
    {
        if (AssetManifest::GetAll().empty() && m_EngineFolders.empty() && m_ProjectFolders.empty())
        {
            ImGui::TextDisabled("No assets in manifest. Click Refresh to scan.");
            return;
        }

        // Filtered: skip the skeleton so only folders containing a match appear (and auto-expand).
        const bool bFiltering = !m_Filter.Text.IsEmpty() || m_Filter.TypeID.IsValid();

        AssetTreeNode lEngineRoot, lProjectRoot;
        BuildRoots(lEngineRoot, lProjectRoot, /*bSeedSkeleton*/!bFiltering, /*bFilterLeaves*/true);

        const bool bEmpty = lEngineRoot.Children.empty()  && lEngineRoot.Leaves.empty()
                         && lProjectRoot.Children.empty() && lProjectRoot.Leaves.empty();
        if (bEmpty)
        {
            ImGui::TextDisabled(bFiltering
                ? "No assets match the current filter."
                : "No assets in manifest. Click Refresh to scan.");
            return;
        }

        DrawFolderNode(lEngineRoot,  OpaaxString(), /*bIsRoot*/true, bFiltering, InSceneMgr, InUIBackend);
        DrawFolderNode(lProjectRoot, OpaaxString(), /*bIsRoot*/true, bFiltering, InSceneMgr, InUIBackend);
    }

    // =============================================================================
    // Folder node — one tree level; recurses into sub-folders then draws leaves
    // =============================================================================
    void AssetBrowserPanel::DrawFolderNode(AssetTreeNode& InNode, const OpaaxString& InParentPath, bool bIsRoot,
                                           bool bFiltering, SceneManager& InSceneMgr, IEditorUIBackend& InUIBackend)
    {
        // A root with nothing under it (e.g. project has no folders/assets yet) is skipped;
        // empty *sub*-folders are kept on purpose so the on-disk structure is visible.
        if (bIsRoot && InNode.Children.empty() && InNode.Leaves.empty()) { return; }

        const OpaaxString lFullPath = InParentPath.IsEmpty() ? InNode.Name : (InParentPath + "/" + InNode.Name);

        ImGuiTreeNodeFlags lFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (bIsRoot) { lFlags |= ImGuiTreeNodeFlags_DefaultOpen; }

        // Active filter → force-open so matches are visible without manual expansion.
        if (bFiltering) { ImGui::SetNextItemOpen(true, ImGuiCond_Always); }

        // Tint the folder label with its assigned color (shared store with the grid view).
        const Uint32 lColor = GetFolderColor(lFullPath);
        if (lColor) { ImGui::PushStyleColor(ImGuiCol_Text, lColor); }
        const bool bOpen = ImGui::TreeNodeEx(InNode.Name.CStr(), lFlags);
        if (lColor) { ImGui::PopStyleColor(); }

        // Right-click a tree folder to set/clear its color too (same store + popup as the grid).
        DrawFolderColorContextMenu(lFullPath);

        if (bOpen)
        {
            for (auto& lChild : InNode.Children)
            {
                DrawFolderNode(lChild, lFullPath, /*bIsRoot*/false, bFiltering, InSceneMgr, InUIBackend);
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

        const char* lIcon = AssetTypeRegistry::GetIcon(InDesc.Type);

        // Label shows the short display name; the ##id keeps it globally unique across folders.
        char lLabel[256];
        snprintf(lLabel, sizeof(lLabel), "%s  %s##%u", lIcon, InDisplayName, InDesc.ID.GetId());

        const bool bClicked = ImGui::Selectable(lLabel, m_HoveredID == InDesc.ID);
        ImGui::PopStyleColor();

        ApplyAssetItemBehavior(InDesc, bClicked, InSceneMgr, InUIBackend);
    }

    // =============================================================================
    // Shared per-asset interactions — attached to the last-submitted item (tree
    // Selectable or grid tile button): select, drag-drop, tooltip, context, double-click
    // =============================================================================
    void AssetBrowserPanel::ApplyAssetItemBehavior(const AssetDescriptor& InDesc, bool bClicked,
                                                   SceneManager& InSceneMgr, IEditorUIBackend& InUIBackend)
    {
        // Scenes don't go through AssetRegistry — treat the entry matching GetCurrentScenePath() as loaded.
        const bool        bLoaded  = AssetRegistry::IsLoaded(InDesc.ID) || IsLoadedScene(InSceneMgr, InDesc);
        const bool        bMissing = InDesc.bMissing;
        const char*       lIcon    = AssetTypeRegistry::GetIcon(InDesc.Type);
        const OpaaxString lIDStr   = InDesc.ID.ToString();

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

        // --- Double-click: invoke the type's activation action (default = Load) ---
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        {
            if (IAssetTypeActions* lActions = AssetTypeRegistry::Find(InDesc.Type))
            {
                lActions->OnActivate(InDesc.ID);
            }
        }
    }

    // =============================================================================
    // Breadcrumb (grid view) — Home > seg > seg ; clicking a crumb truncates the path
    // =============================================================================
    void AssetBrowserPanel::DrawBreadcrumb()
    {
        const bool bHome = ImGui::SmallButton("Home");

        Int32        lNavTo = -1;
        const Uint32 lCount = static_cast<Uint32>(m_GridPath.size());
        for (Uint32 i = 0; i < lCount; ++i)
        {
            ImGui::SameLine();
            ImGui::TextDisabled(">");
            ImGui::SameLine();
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::SmallButton(m_GridPath[i].CStr())) { lNavTo = static_cast<Int32>(i); }
            ImGui::PopID();
        }

        // Apply navigation AFTER the loop — never resize m_GridPath mid-iteration.
        if (bHome)            { m_GridPath.clear(); }
        else if (lNavTo >= 0) { m_GridPath.resize(static_cast<size_t>(lNavTo) + 1); }
    }

    // =============================================================================
    // Asset grid (explorer view) — folders as tiles you double-click to enter
    // =============================================================================
    void AssetBrowserPanel::DrawAssetGrid(SceneManager& InSceneMgr, IEditorUIBackend& InUIBackend)
    {
        AssetTreeNode lEngineRoot, lProjectRoot;
        BuildRoots(lEngineRoot, lProjectRoot, /*bSeedSkeleton*/true, /*bFilterLeaves*/false);

        // Wrapped-grid column count from the available width.
        const ImGuiStyle& lStyle = ImGui::GetStyle();
        const float       lAvail = ImGui::GetContentRegionAvail().x;
        Int32             lCols  = static_cast<Int32>((lAvail + lStyle.ItemSpacing.x) / (k_TileSize + lStyle.ItemSpacing.x));
        if (lCols < 1) { lCols = 1; }

        Int32 lIndex = 0;
        auto  lAfterTile = [&]()
        {
            ++lIndex;
            if (lIndex % lCols != 0) { ImGui::SameLine(); }
        };

        // Roots level: the two roots as folder tiles (full path == the root name).
        if (m_GridPath.empty())
        {
            DrawFolderTile(lEngineRoot.Name,  lEngineRoot.Name,  InUIBackend); lAfterTile();
            DrawFolderTile(lProjectRoot.Name, lProjectRoot.Name, InUIBackend); lAfterTile();
            return;
        }

        // Walk to the current node; self-heal a stale path (folder deleted since the last scan).
        AssetTreeNode* lNode = (m_GridPath[0] == lEngineRoot.Name)  ? &lEngineRoot
                             : (m_GridPath[0] == lProjectRoot.Name) ? &lProjectRoot
                                                                    : nullptr;
        Uint32 lValid = lNode ? 1u : 0u;
        for (Uint32 i = 1; lNode && i < m_GridPath.size(); ++i)
        {
            AssetTreeNode* lChild = FindChildByName(*lNode, m_GridPath[i]);
            if (!lChild) { break; }
            lNode  = lChild;
            lValid = i + 1;
        }
        if (lValid < m_GridPath.size()) { m_GridPath.resize(lValid); }
        if (!lNode) { m_GridPath.clear(); return; }

        // Sub-folders first, then the (filtered) assets in this folder.
        const OpaaxString lDir = JoinPath(m_GridPath);   // current dir (e.g. "Engine/Textures")
        for (auto& lChild : lNode->Children)
        {
            DrawFolderTile(lChild.Name, lDir + "/" + lChild.Name, InUIBackend);
            lAfterTile();
        }
        for (const AssetDescriptor* lLeaf : lNode->Leaves)
        {
            if (!m_Filter.Matches(*lLeaf)) { continue; }
            DrawAssetTile(*lLeaf, InSceneMgr, InUIBackend);
            lAfterTile();
        }

        if (lIndex == 0) { ImGui::TextDisabled("Empty folder."); }
    }

    // =============================================================================
    // Folder tile — vector/PNG folder glyph; double-click enters the folder
    // =============================================================================
    void AssetBrowserPanel::DrawFolderTile(const OpaaxString& InSegment, const OpaaxString& InFullPath, IEditorUIBackend& InUIBackend)
    {
        ImGui::PushID(InSegment.CStr());
        ImGui::BeginGroup();

        const ImVec2 lPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##folder", ImVec2(k_TileSize, k_TileSize));
        const bool bHovered = ImGui::IsItemHovered();
        const bool bDouble  = bHovered && ImGui::IsMouseDoubleClicked(0);

        // Right-click → folder color (Unreal-style); persisted on change.
        DrawFolderColorContextMenu(InFullPath);

        Uint64 lIconTex = 0;
        if (ResolveFolderIcon() && m_FolderIcon.Get()->GetRHITexture())
        {
            lIconTex = InUIBackend.GetTextureID(*m_FolderIcon.Get()->GetRHITexture());
        }

        const Uint32 lColor = GetFolderColor(InFullPath);
        const ImU32  lTint  = lColor ? lColor : k_DefaultFolderColor;
        DrawFolderGlyph(ImGui::GetWindowDrawList(), lPos,
            ImVec2(lPos.x + k_TileSize, lPos.y + k_TileSize), bHovered, lIconTex, lTint);

        DrawTileLabel(InSegment.CStr(), k_TileSize);

        ImGui::EndGroup();
        ImGui::PopID();

        if (bDouble) { m_GridPath.push_back(InSegment); }
    }

    // =============================================================================
    // Asset tile — already-loaded texture thumbnail or a generic glyph; shared behavior
    // =============================================================================
    void AssetBrowserPanel::DrawAssetTile(const AssetDescriptor& InDesc, SceneManager& InSceneMgr, IEditorUIBackend& InUIBackend)
    {
        ImGui::PushID(static_cast<int>(InDesc.ID.GetId()));
        ImGui::BeginGroup();

        const ImVec2 lPos     = ImGui::GetCursorScreenPos();
        const bool   bClicked = ImGui::InvisibleButton("##tile", ImVec2(k_TileSize, k_TileSize));
        const bool   bHovered = ImGui::IsItemHovered();

        // Behavior attaches to the button (the last-submitted item) — the draw-list calls below submit nothing.
        ApplyAssetItemBehavior(InDesc, bClicked, InSceneMgr, InUIBackend);

        ImDrawList*  lDL = ImGui::GetWindowDrawList();
        const ImVec2 lMax(lPos.x + k_TileSize, lPos.y + k_TileSize);
        const bool   bLoaded = AssetRegistry::IsLoaded(InDesc.ID) || IsLoadedScene(InSceneMgr, InDesc);

        // Thumbnail via the type's hook (texture image / font atlas / …); else a generic card glyph.
        // The type owns the look — the panel no longer special-cases any asset type.
        AssetThumbnail     lThumb;
        IAssetTypeActions* lActions  = AssetTypeRegistry::Find(InDesc.Type);
        const bool         bHasThumb = !InDesc.bMissing && lActions
                                    && lActions->GetThumbnail(InDesc.ID, InUIBackend, lThumb) && lThumb.Handle != 0;

        if (bHasThumb)
        {
            if (bHovered) { lDL->AddRectFilled(lPos, lMax, ImGui::GetColorU32(ImGuiCol_HeaderHovered), 4.f); }
            const float lInset = k_TileSize * 0.08f;
            lDL->AddImage(static_cast<ImTextureID>(lThumb.Handle),
                ImVec2(lPos.x + lInset, lPos.y + lInset), ImVec2(lMax.x - lInset, lMax.y - lInset),
                ImVec2(lThumb.UV0.x, lThumb.UV0.y), ImVec2(lThumb.UV1.x, lThumb.UV1.y));
        }
        else
        {
            DrawAssetGlyph(lDL, lPos, lMax, AssetTypeRegistry::GetIcon(InDesc.Type),
                InDesc.bMissing, bLoaded, bHovered);
        }

        // Label colored by load status — green loaded / red missing / white otherwise (parity with the tree).
        ImVec4 lLabelColor;
        if (InDesc.bMissing) { lLabelColor = ImVec4(1.f,  0.3f, 0.3f, 1.f); }
        else if (bLoaded)    { lLabelColor = ImVec4(0.4f, 1.f,  0.4f, 1.f); }
        else                 { lLabelColor = ImVec4(1.f,  1.f,  1.f,  1.f); }
        ImGui::PushStyleColor(ImGuiCol_Text, lLabelColor);
        DrawTileLabel(LeafName(InDesc.ID.ToString()).CStr(), k_TileSize);
        ImGui::PopStyleColor();

        ImGui::EndGroup();
        ImGui::PopID();
    }

    // =============================================================================
    // Folder color picker — right-click popup, shared by grid tile + tree node
    // =============================================================================
    void AssetBrowserPanel::DrawFolderColorContextMenu(const OpaaxString& InFullPath)
    {
        // NULL str_id → the popup is keyed to the last-submitted item (the folder tile button or
        // the tree node), so it's unique per folder with no manual id juggling.
        if (ImGui::BeginPopupContextItem())
        {
            const Uint32 lCur  = GetFolderColor(InFullPath);
            ImVec4       lEdit = ImGui::ColorConvertU32ToFloat4(lCur ? lCur : k_DefaultFolderColor);
            if (ImGui::ColorEdit4("Folder Color", &lEdit.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
            {
                SetFolderColor(InFullPath, ImGui::ColorConvertFloat4ToU32(lEdit));
                SaveFolderColors();
            }
            if (ImGui::MenuItem("Clear Color"))
            {
                ClearFolderColor(InFullPath);
                SaveFolderColors();
            }
            ImGui::EndPopup();
        }
    }

    // =============================================================================
    // ResolveFolderIcon — lazy-load the optional PNG folder icon, once per scan
    // =============================================================================
    bool AssetBrowserPanel::ResolveFolderIcon()
    {
        if (m_FolderIcon.IsValid()) { return true; }
        if (m_FolderIconTried)      { return false; }

        m_FolderIconTried = true;
        const OpaaxStringID lIconID("Textures/Editor/Folder");
        if (AssetManifest::Contains(lIconID))
        {
            m_FolderIcon = AssetRegistry::Load<Texture2D>(lIconID);
        }
        return m_FolderIcon.IsValid();
    }

    // =============================================================================
    // Folder colors — linear store (few folders), persisted to EditorFolderColors.json
    // =============================================================================
    Uint32 AssetBrowserPanel::GetFolderColor(const OpaaxString& InPath) const
    {
        for (const auto& lEntry : m_FolderColors)
        {
            if (lEntry.Path == InPath) { return lEntry.Color; }
        }
        return 0;
    }

    void AssetBrowserPanel::SetFolderColor(const OpaaxString& InPath, Uint32 InColor)
    {
        for (auto& lEntry : m_FolderColors)
        {
            if (lEntry.Path == InPath) { lEntry.Color = InColor; return; }
        }
        m_FolderColors.push_back(FolderColorEntry{ InPath, InColor });
    }

    void AssetBrowserPanel::ClearFolderColor(const OpaaxString& InPath)
    {
        for (auto lIt = m_FolderColors.begin(); lIt != m_FolderColors.end(); ++lIt)
        {
            if (lIt->Path == InPath) { m_FolderColors.erase(lIt); return; }
        }
    }

    void AssetBrowserPanel::LoadFolderColors()
    {
        m_FolderColors.clear();

        const OpaaxString lPath = OpaaxPath::ToAbsolute("EditorFolderColors.json");
        std::ifstream     lFile(lPath.CStr());
        if (!lFile.is_open()) { return; }

        nlohmann::json lRoot;
        try { lFile >> lRoot; }
        catch (...)
        {
            OPAAX_CORE_WARN("AssetBrowserPanel - failed to parse '{}', ignoring folder colors.", lPath);
            return;
        }

        if (!lRoot.contains("folders") || !lRoot["folders"].is_array()) { return; }

        for (const auto& lEntry : lRoot["folders"])
        {
            if (!lEntry.contains("path") || !lEntry.contains("color")) { continue; }
            const auto& lC = lEntry["color"];
            if (!lC.is_array() || lC.size() < 4) { continue; }

            const int lR = lC[0].get<int>(), lG = lC[1].get<int>(), lB = lC[2].get<int>(), lA = lC[3].get<int>();
            m_FolderColors.push_back(FolderColorEntry{
                OpaaxString(lEntry["path"].get<std::string>().c_str()),
                static_cast<Uint32>(IM_COL32(lR, lG, lB, lA)) });
        }
    }

    void AssetBrowserPanel::SaveFolderColors() const
    {
        nlohmann::json lRoot;
        lRoot["folders"] = nlohmann::json::array();

        for (const auto& lEntry : m_FolderColors)
        {
            const Uint32 lC = lEntry.Color;
            nlohmann::json lJson;
            lJson["path"]  = lEntry.Path.CStr();
            lJson["color"] = { (lC >> IM_COL32_R_SHIFT) & 0xFF, (lC >> IM_COL32_G_SHIFT) & 0xFF,
                               (lC >> IM_COL32_B_SHIFT) & 0xFF, (lC >> IM_COL32_A_SHIFT) & 0xFF };
            lRoot["folders"].push_back(lJson);
        }

        const OpaaxString lPath = OpaaxPath::ToAbsolute("EditorFolderColors.json");
        std::ofstream     lFile(lPath.CStr());
        if (lFile.is_open()) { lFile << lRoot.dump(4); }
        else { OPAAX_CORE_ERROR("AssetBrowserPanel - cannot write folder colors to '{}'", lPath); }
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
