#include "Editor/Panels/ResourceBrowserPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/Application/Services/EditorPaths.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Extensions/ResourceTypeRegistry.h"
#include "Editor/ImguiLibrary/ImguiDraw.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Resources/ResourceDragDrop.h"

#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Platform/IFileSystem.h"
#include "Engine/Registries/EngineRegistries.h"   // extension -> resource type, the engine's half
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/ResourceTypeID.hpp"        // which type a thumbnail is for
#include "Engine/Subsystems/Resources/Types/Texture/TextureResource.h"   // icons are textures like any other

#include <imgui.h>

#include <cstring>

using namespace Opaax;

namespace
{
    constexpr float k_TileSize      = 84.f;
    constexpr char  k_UnknownGlyph[] = "[ ? ]";

    // The icon column in a Tree/List row: one line tall, so a row keeps its height.
    float RowIconSize() { return ImGui::GetTextLineHeight(); }
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
        m_Roots.emplace_back(OPAAX_ID("Project"), m_Context.Paths.AssetsDir());

        // The engine's shipped content. Browsable AND draggable: AbsoluteToAsset names a file under
        // it by its "/Engine/" mount, so a component can reference one (IPaths::ENGINE_MOUNT).
        m_Roots.emplace_back(OPAAX_ID("Engine"), m_Context.Paths.EngineAssetsDir());

        // The editor's own per-project space. Absent when no edited project was declared — then there
        // is simply one root fewer, which every view already handles.
        if (m_Context.EditorPathsOrNull != nullptr)
        {
            m_Roots.emplace_back(OPAAX_ID("Editor"), m_Context.EditorPathsOrNull->EditorAssetsDir());
        }

        LoadTypeIcons();
        Refresh();
    }

    void ResourceBrowserPanel::Shutdown()
    {
        // Before the ResourceManager's own flush and while the GL context is alive, so the icon
        // textures are deleted on a live device (LC3).
        m_TypeIcons.clear();
    }

    void ResourceBrowserPanel::Refresh()
    {
        for (ResourceRoot& lRoot : m_Roots)
        {
            if (ScanRoot(m_Context.FileSystem, lRoot))
            {
                OPAAX_LOG(LogResourceBrowserPanel, Info, "Resource browser scanned '{}' ({}): {} files in {} folders",
                    lRoot.Label, lRoot.AbsPath.CStr(), lRoot.FileCount, lRoot.FolderCount);
            }
            else
            {
                OPAAX_LOG(LogResourceBrowserPanel, Warn, "Resource browser root '{}' not found: {}",
                    lRoot.Label, lRoot.AbsPath.CStr());
            }
        }
    }

    void ResourceBrowserPanel::DrawContents()
    {
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
        if (ImguiWidgets::ToggleButton("Tiles", m_View == EBrowserView::Tiles)) { m_View = EBrowserView::Tiles; }
        ImGui::SameLine();
        if (ImguiWidgets::ToggleButton("Tree",  m_View == EBrowserView::Tree))  { m_View = EBrowserView::Tree; }
        ImGui::SameLine();
        if (ImguiWidgets::ToggleButton("List",  m_View == EBrowserView::List))  { m_View = EBrowserView::List; }

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
                if (bEnter) { m_TilePath.emplace_back(lLabel); }

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
            if (bEnter) { m_TilePath.emplace_back(lChild.Name); }

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

        ImDrawList*  lDrawList = ImGui::GetWindowDrawList();
        const ImVec2 lMax(lPos.x + k_TileSize, lPos.y + k_TileSize);

        if (bHovered) { ImguiDraw::HoverHighlight(lDrawList, lPos, lMax); }
        ImguiDraw::FolderGlyph(lDrawList, lPos, lMax);
        ImguiWidgets::TextEllipsized(InName.CStr(), k_TileSize, /*bInCentered*/true);

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

        const FileType lType     = FindType(InFile);
        ImDrawList*    lDrawList = ImGui::GetWindowDrawList();
        const ImVec2   lMax(lPos.x + k_TileSize, lPos.y + k_TileSize);

        if (bHovered) { ImguiDraw::HoverHighlight(lDrawList, lPos, lMax); }
        ImguiDraw::IconBox(lDrawList, TileImageOf(InFile, lType), GlyphOf(lType), lPos, lMax);

        const bool bSelected = (m_SelectedPath == FullPathOf(InFile, InRoot));
        if (bSelected) { ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.26f, 0.59f, 0.98f, 1.f)); }
        ImguiWidgets::TextEllipsized(InFile.Name.CStr(), k_TileSize, /*bInCentered*/true);
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
                    lRoot.Label.CStr(), lRoot.AbsPath.CStr());
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
        const FileType lType     = FindType(InFile);
        const float    lIconSize = RowIconSize();

        // Indented past the icon column, then the ICON IS PAINTED OVER that indent on the draw list.
        // Painting rather than laying out is what keeps the Selectable the last-submitted item, so
        // ApplyFileBehavior's hover, click and drag-drop all still apply to the whole row.
        char lLabel[512];
        snprintf(lLabel, sizeof(lLabel), "      %s", InDisplayName);

        ImGui::PushID(InFile.RelPath.CStr());

        const ImVec2 lPos     = ImGui::GetCursorScreenPos();
        const bool   bClicked = ImGui::Selectable(lLabel, m_SelectedPath == FullPathOf(InFile, InRoot));
        ApplyFileBehavior(InFile, InRoot, bClicked);

        // No inset for a row: the icon is already only one line tall, so IconBox's tile inset would
        // shrink it to nothing.
        ImguiDraw::IconBox(ImGui::GetWindowDrawList(), TileImageOf(InFile, lType), GlyphOf(lType),
                           lPos, ImVec2(lPos.x + lIconSize, lPos.y + lIconSize), /*InInset*/0.f);

        ImGui::PopID();
    }

    void ResourceBrowserPanel::ApplyFileBehavior(const ResourceFile& InFile, const ResourceRoot& InRoot, bool bClicked)
    {
        const OpaaxString lFullPath = FullPathOf(InFile, InRoot);
        const FileType    lType     = FindType(InFile);

        if (bClicked)
        {
            m_SelectedPath = lFullPath;

            // Discrete (a click), so no spam — and the only signal that a selection moved at all.
            OPAAX_LOG(LogResourceBrowserPanel, Info, "Resource browser selected '{}'", lFullPath.CStr());
        }

        // Drag it into a field. GENERIC: the type comes from the engine's format table, so every
        // registered resource is draggable and adding one touches nothing here. A file outside every
        // mount converts to an empty path and simply carries no payload — a real answer, not a failure.
        if (lType.Format != nullptr && ImGui::BeginDragDropSource())
        {
            SetResourceDragPayload(lType.Format->TypeId, m_Context.Paths.AbsoluteToAsset(InFile.AbsPath));

            const float lIconSize = RowIconSize();
            ImguiWidgets::Image(TileImageOf(InFile, lType), ImVec2(lIconSize, lIconSize));
            ImGui::SameLine();
            ImGui::TextUnformatted(InFile.Name.CStr());

            ImGui::EndDragDropSource();
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::TextDisabled("Name : %s", InFile.Name.CStr());
            ImGui::TextDisabled("Path : %s", lFullPath.CStr());
            ImGui::TextDisabled("Type : %s", LabelOf(lType));
            ImGui::EndTooltip();

            if (ImGui::IsMouseDoubleClicked(0))
            {
                if (lType.Format == nullptr)
                {
                    OPAAX_LOG(LogResourceBrowserPanel, Info, "'{}' activated — no resource type registered for '{}'",
                        InFile.Name.CStr(),
                        InFile.Extension.IsValid() ? InFile.Extension.CStr() : "(no extension)");
                }
                else if (lType.Chrome == nullptr || !lType.Chrome->OnActivate)
                {
                    OPAAX_LOG(LogResourceBrowserPanel, Info, "'{}' activated — type '{}' registers no action",
                        InFile.Name.CStr(), LabelOf(lType));
                }
                else
                {
                    lType.Chrome->OnActivate(m_Context, InFile);
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

    ResourceBrowserPanel::FileType ResourceBrowserPanel::FindType(const ResourceFile& InFile) const
    {
        FileType lType;

        lType.Format = m_Context.Engine.GetRegistries().Resources().FindByExtension(InFile.Extension);
        if (lType.Format != nullptr)
        {
            lType.Chrome = m_Context.Extensions.ResourceTypes().Find(lType.Format->TypeId);
        }

        return lType;
    }

    const char* ResourceBrowserPanel::LabelOf(const FileType& InType)
    {
        if (InType.Chrome != nullptr && InType.Chrome->Label.IsValid())
        {
            return InType.Chrome->Label.CStr();
        }

        return (InType.Format != nullptr) ? InType.Format->Format->Label : "Unknown type";
    }

    const char* ResourceBrowserPanel::GlyphOf(const FileType& InType)
    {
        return (InType.Chrome != nullptr && !InType.Chrome->Glyph.IsEmpty())
                   ? InType.Chrome->Glyph.CStr()
                   : k_UnknownGlyph;
    }

    // =============================================================================
    // Icons
    // =============================================================================
    OpaaxString ResourceBrowserPanel::ResolveIconPath(const OpaaxString& InIconRel) const
    {
        if (m_Context.EditorPathsOrNull == nullptr)
        {
            return OpaaxString();   // no editor space at all — the glyph is the whole answer
        }

        const OpaaxString lProjectIcon = m_Context.EditorPathsOrNull->EditorAssetToAbsolute(InIconRel);
        if (m_Context.FileSystem.IsPathExist(lProjectIcon))
        {
            return lProjectIcon;
        }

        return m_Context.EditorPathsOrNull->ToolAssetToAbsolute(InIconRel);
    }

    void ResourceBrowserPanel::LoadTypeIcons()
    {
        // EAGER, and the registry is what makes that the right call: ResourceTypes() was SEALED at
        // OnModulesRegistered, so the set of icons is finite and known right here. Loading on first
        // draw instead would spread a handful of tiny reads over arbitrary frames and report a
        // missing file at whichever one happened to show it.
        for (const ResourceTypeDesc& lChrome : m_Context.Extensions.ResourceTypes().Entries())
        {
            if (lChrome.Icon.IsEmpty())
            {
                continue;   // glyph-only, a perfectly good registration
            }

            const OpaaxString lAbsolute = ResolveIconPath(lChrome.Icon);
            ResourceRef<TextureResource> lRef = lAbsolute.IsEmpty()
                                                    ? ResourceRef<TextureResource>{}
                                                    : m_Context.Resources.Load<TextureResource>(lAbsolute.CStr());

            // Logged on BOTH branches: a cache that only reports failures cannot be told apart from
            // one that never ran (the RendererManager texture cache's rule).
            //
            // A FAILED ref is DROPPED rather than cached, and that is not tidiness: ResourceRef::Get
            // on a failed claim answers the PLACEHOLDER, so storing it would draw a magenta square
            // where the glyph belongs. Nothing is retried either way — this runs once.
            if (!lRef.IsValid())
            {
                OPAAX_LOG(LogResourceBrowserPanel, Warn, "Icon '{}' did not load — that type draws its glyph",
                    lChrome.Icon.CStr());
                continue;
            }

            OPAAX_LOG(LogResourceBrowserPanel, Info, "Icon loaded: {}", lAbsolute.CStr());
            m_TypeIcons.emplace(lChrome.TypeId, Move(lRef));
        }
    }

    EditorImage ResourceBrowserPanel::IconOf(const FileType& InType) const
    {
        if (InType.Chrome == nullptr)
        {
            return EditorImage{};
        }

        const auto lIt = m_TypeIcons.find(InType.Chrome->TypeId);
        if (lIt == m_TypeIcons.end())
        {
            return EditorImage{};
        }

        const TextureResource* lIcon = lIt->second.Get();
        if (lIcon == nullptr || lIcon->GetTexture() == nullptr)
        {
            return EditorImage{};
        }

        return m_Context.UIBackend.GetTextureImage(*lIcon->GetTexture());
    }

    EditorImage ResourceBrowserPanel::TileImageOf(const ResourceFile& InFile, const FileType& InType) const
    {
        // An image file shows ITSELF once something has loaded it. Find is the whole reason this is
        // affordable: it answers "already resident?" and never turns into a load, so scrolling a
        // folder cannot pull it into memory (Legacy's rule, kept).
        //
        // The claim is taken and dropped within the frame — held only so the payload cannot be
        // collected between the question and the draw.
        if (InType.Format != nullptr && InType.Format->TypeId == ResourceTypeID::Get<TextureResource>())
        {
            const ResourceRef<TextureResource> lLoaded =
                m_Context.Resources.Find<TextureResource>(InFile.AbsPath.CStr());

            if (const TextureResource* lTexture = lLoaded.IsValid() ? lLoaded.Get() : nullptr;
                lTexture != nullptr && lTexture->GetTexture() != nullptr)
            {
                return m_Context.UIBackend.GetTextureImage(*lTexture->GetTexture());
            }
        }

        return IconOf(InType);
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
