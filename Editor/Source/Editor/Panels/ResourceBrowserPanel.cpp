#include "Editor/Panels/ResourceBrowserPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/Application/Services/EditorPaths.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Extensions/ResourceTypeRegistry.h"

#include "Application/Services/IPaths.h"
#include "Application/Services/Platforms/IFileSystem.h"

#include <imgui.h>

#include <cstring>

using namespace Opaax;

namespace
{
    constexpr float k_TileSize     = 84.f;
    constexpr ImU32 k_FolderColor  = IM_COL32(232, 196, 104, 255);   // gold, Explorer-ish
    constexpr char  k_UnknownIcon[] = "[ ? ]";

    // Scale a packed color's RGB (alpha preserved) — the folder glyph's darker tab.
    ImU32 DarkenColor(ImU32 InColor, float InFactor)
    {
        const int lR = static_cast<int>(((InColor >> IM_COL32_R_SHIFT) & 0xFF) * InFactor);
        const int lG = static_cast<int>(((InColor >> IM_COL32_G_SHIFT) & 0xFF) * InFactor);
        const int lB = static_cast<int>(((InColor >> IM_COL32_B_SHIFT) & 0xFF) * InFactor);
        const int lA =  (InColor >> IM_COL32_A_SHIFT) & 0xFF;
        return IM_COL32(lR, lG, lB, lA);
    }

    // A folder: body + tab. Drawn on the window draw list, so it submits no item of its own — the
    // caller's InvisibleButton stays the last-submitted item and keeps every interaction.
    void DrawFolderGlyph(ImDrawList* InDrawList, ImVec2 InMin, ImVec2 InMax, bool bHovered)
    {
        if (bHovered)
        {
            InDrawList->AddRectFilled(InMin, InMax, ImGui::GetColorU32(ImGuiCol_HeaderHovered), 4.f);
        }

        const float  lWidth  = InMax.x - InMin.x;
        const float  lHeight = InMax.y - InMin.y;
        const ImVec2 lA(InMin.x + lWidth * 0.16f, InMin.y + lHeight * 0.22f);
        const ImVec2 lB(InMax.x - lWidth * 0.16f, InMax.y - lHeight * 0.22f);
        const float  lTabH   = (lB.y - lA.y) * 0.26f;
        const float  lTabW   = (lB.x - lA.x) * 0.30f;

        InDrawList->AddRectFilled(lA, ImVec2(lA.x + lTabW, lA.y + lTabH + 3.f),
            DarkenColor(k_FolderColor, 0.84f), 3.f, ImDrawFlags_RoundCornersTop);
        InDrawList->AddRectFilled(ImVec2(lA.x, lA.y + lTabH), lB, k_FolderColor, 3.f);
    }

    // A file: a card with the type's icon glyph centered in it.
    void DrawFileGlyph(ImDrawList* InDrawList, ImVec2 InMin, ImVec2 InMax, const char* InIcon, bool bHovered)
    {
        if (bHovered)
        {
            InDrawList->AddRectFilled(InMin, InMax, ImGui::GetColorU32(ImGuiCol_HeaderHovered), 4.f);
        }

        const ImVec2 lA(InMin.x + (InMax.x - InMin.x) * 0.16f, InMin.y + (InMax.y - InMin.y) * 0.16f);
        const ImVec2 lB(InMax.x - (InMax.x - InMin.x) * 0.16f, InMax.y - (InMax.y - InMin.y) * 0.16f);

        InDrawList->AddRectFilled(lA, lB, IM_COL32(80, 80, 92, 255), 4.f);
        InDrawList->AddRect(lA, lB, IM_COL32(0, 0, 0, 120), 4.f);

        const ImVec2 lTextSize = ImGui::CalcTextSize(InIcon);
        InDrawList->AddText(ImVec2((lA.x + lB.x) * 0.5f - lTextSize.x * 0.5f,
                                   (lA.y + lB.y) * 0.5f - lTextSize.y * 0.5f),
            IM_COL32(232, 232, 232, 255), InIcon);
    }

    // One centered line under a tile, ellipsized when it overflows — a tile grid is unreadable if long
    // names are allowed to set the column width.
    void DrawTileLabel(const char* InText, float InWidth)
    {
        const ImVec2 lFull = ImGui::CalcTextSize(InText);
        if (lFull.x <= InWidth)
        {
            const float lOffset = (InWidth - lFull.x) * 0.5f;
            if (lOffset > 0.f) { ImGui::SetCursorPosX(ImGui::GetCursorPosX() + lOffset); }
            ImGui::TextUnformatted(InText);
            return;
        }

        char         lBuffer[160];
        const float  lDotsWidth = ImGui::CalcTextSize("..").x;
        const size_t lMax       = strlen(InText);
        size_t       lLength    = 0;
        float        lWidth     = 0.f;

        for (; lLength < lMax && lLength < sizeof(lBuffer) - 3; ++lLength)
        {
            const char lChar[2] = { InText[lLength], '\0' };
            lWidth += ImGui::CalcTextSize(lChar).x;
            if (lWidth + lDotsWidth > InWidth) { break; }
        }

        memcpy(lBuffer, InText, lLength);
        lBuffer[lLength]     = '.';
        lBuffer[lLength + 1] = '.';
        lBuffer[lLength + 2] = '\0';
        ImGui::TextUnformatted(lBuffer);
    }

    // A view toggle button, tinted while it is the active view.
    bool ViewButton(const char* InLabel, bool bActive)
    {
        if (bActive) { ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.f)); }
        const bool bPressed = ImGui::SmallButton(InLabel);
        if (bActive) { ImGui::PopStyleColor(); }
        return bPressed;
    }
}

namespace Opaax::Editor
{
    ResourceBrowserPanel::ResourceBrowserPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    ResourceBrowserPanel::~ResourceBrowserPanel() = default;

    // =============================================================================
    // Lifecycle
    // =============================================================================
    void ResourceBrowserPanel::Startup()
    {
        m_Roots.push_back(ResourceRoot{ OPAAX_ID("Project"), m_Context.Paths.AssetsDir() });

        // The editor's own per-project space. Absent when no edited project was declared — then there
        // is simply one root, which every view already handles.
        if (m_Context.EditorPathsOrNull != nullptr)
        {
            m_Roots.push_back(ResourceRoot{ OPAAX_ID("Editor"), m_Context.EditorPathsOrNull->EditorAssetsDir() });
        }

        Refresh();
    }

    void ResourceBrowserPanel::Refresh()
    {
        for (ResourceRoot& lRoot : m_Roots)
        {
            if (ScanRoot(m_Context.FileSystem, lRoot))
            {
                OPAAX_LOG(LogResourceBrowserPanel, Info, "Resource browser scanned '{}' ({}): {} files in {} folders",
                    lRoot.Label.ToString().CStr(), lRoot.AbsPath.CStr(), lRoot.FileCount, lRoot.FolderCount);
            }
            else
            {
                OPAAX_LOG(LogResourceBrowserPanel, Warn, "Resource browser root '{}' not found: {}",
                    lRoot.Label.ToString().CStr(), lRoot.AbsPath.CStr());
            }
        }
    }

    void ResourceBrowserPanel::Draw()
    {
        ImGui::SetNextWindowSize(ImVec2(520.f, 320.f), ImGuiCond_FirstUseEver);
        ImGui::Begin(m_Title.CStr());

        // Frozen header: the toolbar (and the breadcrumb in Tiles) stay pinned while only the content
        // area below scrolls — Excel's "freeze panes", salvaged from the old browser.
        DrawToolbar();
        if (m_View == EBrowserView::Tiles) { DrawBreadcrumb(); }
        ImGui::Separator();

        ImGui::BeginChild("##ResourceScroll", ImVec2(0.f, 0.f), false);
        switch (m_View)
        {
            case EBrowserView::Tiles: DrawTiles(); break;
            case EBrowserView::Tree:  DrawTree();  break;
            case EBrowserView::List:  DrawList();  break;
        }
        ImGui::EndChild();

        ImGui::End();
    }

    // =============================================================================
    // Toolbar + breadcrumb
    // =============================================================================
    void ResourceBrowserPanel::DrawToolbar()
    {
        if (ImGui::Button("Refresh"))
        {
            Refresh();
        }

        ImGui::SameLine();
        if (ViewButton("Tiles", m_View == EBrowserView::Tiles)) { m_View = EBrowserView::Tiles; }
        ImGui::SameLine();
        if (ViewButton("Tree",  m_View == EBrowserView::Tree))  { m_View = EBrowserView::Tree; }
        ImGui::SameLine();
        if (ViewButton("List",  m_View == EBrowserView::List))  { m_View = EBrowserView::List; }

        char lBuffer[128];
        strncpy_s(lBuffer, sizeof(lBuffer), m_Filter.CStr(), _TRUNCATE);
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::InputTextWithHint("##ResourceFilter", "Filter by name...", lBuffer, sizeof(lBuffer)))
        {
            m_Filter = OpaaxString(lBuffer);
        }
    }

    void ResourceBrowserPanel::DrawBreadcrumb()
    {
        const bool bHome = ImGui::SmallButton("Home");

        Int32 lNavigateTo = -1;
        for (Uint32 i = 0; i < static_cast<Uint32>(m_TilePath.size()); ++i)
        {
            ImGui::SameLine();
            ImGui::TextDisabled(">");
            ImGui::SameLine();

            ImGui::PushID(static_cast<int>(i));
            if (ImGui::SmallButton(m_TilePath[i].CStr())) { lNavigateTo = static_cast<Int32>(i); }
            ImGui::PopID();
        }

        // Applied AFTER the loop — never resize the path being iterated.
        if (bHome)                 { m_TilePath.clear(); }
        else if (lNavigateTo >= 0) { m_TilePath.resize(static_cast<size_t>(lNavigateTo) + 1); }
    }

    // =============================================================================
    // Tiles
    // =============================================================================
    void ResourceBrowserPanel::DrawTiles()
    {
        const ImGuiStyle& lStyle     = ImGui::GetStyle();
        const float       lAvailable = ImGui::GetContentRegionAvail().x;

        Int32 lColumns = static_cast<Int32>((lAvailable + lStyle.ItemSpacing.x) / (k_TileSize + lStyle.ItemSpacing.x));
        if (lColumns < 1) { lColumns = 1; }

        Int32 lIndex     = 0;
        auto  lAfterTile = [&]()
        {
            ++lIndex;
            if (lIndex % lColumns != 0) { ImGui::SameLine(); }
        };

        // Home: the roots themselves are the tiles.
        if (m_TilePath.empty())
        {
            for (const ResourceRoot& lRoot : m_Roots)
            {
                const OpaaxString lLabel = lRoot.Label.ToString();

                bool bEnter = false;
                DrawFolderTile(lLabel, bEnter);
                if (bEnter) { m_TilePath.push_back(lLabel); }

                lAfterTile();
            }

            if (lIndex == 0) { ImGui::TextDisabled("No resource roots configured."); }
            return;
        }

        const ResourceRoot*   lRoot   = ResolveTileRoot();
        const ResourceFolder* lFolder = ResolveTileFolder();
        if (lRoot == nullptr || lFolder == nullptr)
        {
            return;   // the path self-healed to Home; next frame renders the roots
        }

        if (!lRoot->bExists)
        {
            ImGui::TextDisabled("Folder not found: %s", lRoot->AbsPath.CStr());
            return;
        }

        for (const ResourceFolder& lChild : lFolder->Folders)
        {
            bool bEnter = false;
            DrawFolderTile(lChild.Name, bEnter);
            if (bEnter) { m_TilePath.push_back(lChild.Name); }

            lAfterTile();
        }

        for (const ResourceFile& lFile : lFolder->Files)
        {
            if (!MatchesFilter(lFile)) { continue; }

            DrawFileTile(lFile, *lRoot);
            lAfterTile();
        }

        if (lIndex == 0)
        {
            ImGui::TextDisabled(m_Filter.IsEmpty() ? "Empty folder." : "No resources match the current filter.");
        }
    }

    void ResourceBrowserPanel::DrawFolderTile(const OpaaxString& InName, bool& bOutEnter)
    {
        ImGui::PushID(InName.CStr());
        ImGui::BeginGroup();

        const ImVec2 lPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##folder", ImVec2(k_TileSize, k_TileSize));

        const bool bHovered = ImGui::IsItemHovered();
        bOutEnter = bHovered && ImGui::IsMouseDoubleClicked(0);

        DrawFolderGlyph(ImGui::GetWindowDrawList(), lPos, ImVec2(lPos.x + k_TileSize, lPos.y + k_TileSize), bHovered);
        DrawTileLabel(InName.CStr(), k_TileSize);

        ImGui::EndGroup();
        ImGui::PopID();
    }

    void ResourceBrowserPanel::DrawFileTile(const ResourceFile& InFile, const ResourceRoot& InRoot)
    {
        ImGui::PushID(InFile.RelPath.CStr());
        ImGui::BeginGroup();

        const ImVec2 lPos     = ImGui::GetCursorScreenPos();
        const bool   bClicked = ImGui::InvisibleButton("##file", ImVec2(k_TileSize, k_TileSize));
        const bool   bHovered = ImGui::IsItemHovered();

        // Attach behavior to the button (the last-submitted item) BEFORE the draw-list calls, which
        // submit nothing of their own.
        ApplyFileBehavior(InFile, InRoot, bClicked);

        const ResourceTypeDesc* lType = FindType(InFile);
        DrawFileGlyph(ImGui::GetWindowDrawList(), lPos, ImVec2(lPos.x + k_TileSize, lPos.y + k_TileSize),
            lType != nullptr ? lType->Icon.CStr() : k_UnknownIcon, bHovered);

        const bool bSelected = (m_SelectedPath == FullPathOf(InFile, InRoot));
        if (bSelected) { ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.26f, 0.59f, 0.98f, 1.f)); }
        DrawTileLabel(InFile.Name.CStr(), k_TileSize);
        if (bSelected) { ImGui::PopStyleColor(); }

        ImGui::EndGroup();
        ImGui::PopID();
    }

    // =============================================================================
    // Tree
    // =============================================================================
    void ResourceBrowserPanel::DrawTree()
    {
        Uint64 lVisibleRoots = 0;

        for (const ResourceRoot& lRoot : m_Roots)
        {
            if (!lRoot.bExists)
            {
                ImGui::TextDisabled("%s — folder not found: %s",
                    lRoot.Label.ToString().CStr(), lRoot.AbsPath.CStr());
                continue;
            }

            if (!m_Filter.IsEmpty() && !FolderHasMatch(lRoot.Tree)) { continue; }

            DrawFolderNode(lRoot.Tree, lRoot, /*bIsRoot*/true);
            ++lVisibleRoots;
        }

        if (lVisibleRoots == 0 && !m_Filter.IsEmpty())
        {
            ImGui::TextDisabled("No resources match the current filter.");
        }
    }

    void ResourceBrowserPanel::DrawFolderNode(const ResourceFolder& InFolder, const ResourceRoot& InRoot, bool bIsRoot)
    {
        const bool bFiltering = !m_Filter.IsEmpty();

        // A filtered branch with nothing matching under it is hidden entirely, rather than expanding
        // into a chain of empty folders.
        if (bFiltering && !FolderHasMatch(InFolder)) { return; }

        ImGuiTreeNodeFlags lFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (bIsRoot) { lFlags |= ImGuiTreeNodeFlags_DefaultOpen; }

        // Filtering force-opens, so a match is never hidden behind a collapsed parent.
        if (bFiltering) { ImGui::SetNextItemOpen(true, ImGuiCond_Always); }

        ImGui::PushID(InFolder.RelPath.IsEmpty() ? InFolder.Name.CStr() : InFolder.RelPath.CStr());
        const bool bOpen = ImGui::TreeNodeEx(InFolder.Name.CStr(), lFlags);

        if (bOpen)
        {
            for (const ResourceFolder& lChild : InFolder.Folders)
            {
                DrawFolderNode(lChild, InRoot, /*bIsRoot*/false);
            }

            for (const ResourceFile& lFile : InFolder.Files)
            {
                if (!MatchesFilter(lFile)) { continue; }
                DrawFileRow(lFile, InRoot, lFile.Name.CStr());
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    // =============================================================================
    // List
    // =============================================================================
    void ResourceBrowserPanel::DrawList()
    {
        Uint64 lRowCount = 0;

        for (const ResourceRoot& lRoot : m_Roots)
        {
            if (!lRoot.bExists) { continue; }
            DrawListFolder(lRoot.Tree, lRoot, lRowCount);
        }

        if (lRowCount == 0)
        {
            ImGui::TextDisabled(m_Filter.IsEmpty()
                ? "No files under the configured roots."
                : "No resources match the current filter.");
        }
    }

    void ResourceBrowserPanel::DrawListFolder(const ResourceFolder& InFolder, const ResourceRoot& InRoot, Uint64& InOutRowCount)
    {
        for (const ResourceFile& lFile : InFolder.Files)
        {
            if (!MatchesFilter(lFile)) { continue; }

            // The full path is the row's whole point here — a flat list of bare names would be ambiguous.
            const OpaaxString lFullPath = FullPathOf(lFile, InRoot);
            DrawFileRow(lFile, InRoot, lFullPath.CStr());
            ++InOutRowCount;
        }

        for (const ResourceFolder& lChild : InFolder.Folders)
        {
            DrawListFolder(lChild, InRoot, InOutRowCount);
        }
    }

    // =============================================================================
    // Shared row + behavior
    // =============================================================================
    void ResourceBrowserPanel::DrawFileRow(const ResourceFile& InFile, const ResourceRoot& InRoot, const char* InDisplayName)
    {
        const ResourceTypeDesc* lType = FindType(InFile);

        char lLabel[512];
        snprintf(lLabel, sizeof(lLabel), "%s  %s",
            lType != nullptr ? lType->Icon.CStr() : k_UnknownIcon, InDisplayName);

        ImGui::PushID(InFile.RelPath.CStr());
        const bool bClicked = ImGui::Selectable(lLabel, m_SelectedPath == FullPathOf(InFile, InRoot));
        ApplyFileBehavior(InFile, InRoot, bClicked);
        ImGui::PopID();
    }

    void ResourceBrowserPanel::ApplyFileBehavior(const ResourceFile& InFile, const ResourceRoot& InRoot, bool bClicked)
    {
        const OpaaxString             lFullPath = FullPathOf(InFile, InRoot);
        const ResourceTypeDesc* const lType     = FindType(InFile);

        if (bClicked)
        {
            m_SelectedPath = lFullPath;

            // Discrete (a click), so no spam — and the only signal that a selection moved at all.
            OPAAX_LOG(LogResourceBrowserPanel, Info, "Resource browser selected '{}'", lFullPath.CStr());
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::TextDisabled("Name : %s", InFile.Name.CStr());
            ImGui::TextDisabled("Path : %s", lFullPath.CStr());
            ImGui::TextDisabled("Type : %s", lType != nullptr ? lType->Label.ToString().CStr() : "Unknown type");
            ImGui::EndTooltip();

            if (ImGui::IsMouseDoubleClicked(0))
            {
                if (lType == nullptr)
                {
                    OPAAX_LOG(LogResourceBrowserPanel, Info, "'{}' activated — no resource type registered for '{}'",
                        InFile.Name.CStr(),
                        InFile.Extension.IsValid() ? InFile.Extension.ToString().CStr() : "(no extension)");
                }
                else if (!lType->OnActivate)
                {
                    OPAAX_LOG(LogResourceBrowserPanel, Info, "'{}' activated — type '{}' registers no action",
                        InFile.Name.CStr(), lType->Label.ToString().CStr());
                }
                else
                {
                    lType->OnActivate(m_Context, InFile);
                }
            }
        }
    }

    // =============================================================================
    // Queries
    // =============================================================================
    OpaaxString ResourceBrowserPanel::FullPathOf(const ResourceFile& InFile, const ResourceRoot& InRoot) const
    {
        return InRoot.Label.ToString() + "/" + InFile.RelPath;
    }

    const ResourceTypeDesc* ResourceBrowserPanel::FindType(const ResourceFile& InFile) const
    {
        return m_Context.Extensions.ResourceTypes().Find(InFile.Extension);
    }

    bool ResourceBrowserPanel::MatchesFilter(const ResourceFile& InFile) const
    {
        if (m_Filter.IsEmpty()) { return true; }

        return InFile.Name.ToLower().Find(m_Filter.ToLower().CStr()) != -1;
    }

    bool ResourceBrowserPanel::FolderHasMatch(const ResourceFolder& InFolder) const
    {
        for (const ResourceFile& lFile : InFolder.Files)
        {
            if (MatchesFilter(lFile)) { return true; }
        }

        for (const ResourceFolder& lChild : InFolder.Folders)
        {
            if (FolderHasMatch(lChild)) { return true; }
        }

        return false;
    }

    const ResourceRoot* ResourceBrowserPanel::ResolveTileRoot() const
    {
        if (m_TilePath.empty()) { return nullptr; }

        for (const ResourceRoot& lRoot : m_Roots)
        {
            if (lRoot.Label == m_TilePath[0]) { return &lRoot; }
        }

        return nullptr;
    }

    const ResourceFolder* ResourceBrowserPanel::ResolveTileFolder()
    {
        const ResourceRoot* lRoot = ResolveTileRoot();
        if (lRoot == nullptr)
        {
            m_TilePath.clear();   // a root that no longer exists strands the view otherwise
            return nullptr;
        }

        const ResourceFolder* lFolder = &lRoot->Tree;
        Uint32                lValid  = 1;

        for (Uint32 i = 1; i < static_cast<Uint32>(m_TilePath.size()); ++i)
        {
            const ResourceFolder* lChild = nullptr;
            for (const ResourceFolder& lCandidate : lFolder->Folders)
            {
                if (lCandidate.Name == m_TilePath[i]) { lChild = &lCandidate; break; }
            }

            if (lChild == nullptr) { break; }

            lFolder = lChild;
            lValid  = i + 1;
        }

        // Truncate at the first segment that stopped resolving (folder deleted since the last scan).
        if (lValid < static_cast<Uint32>(m_TilePath.size()))
        {
            m_TilePath.resize(lValid);
        }

        return lFolder;
    }
}
