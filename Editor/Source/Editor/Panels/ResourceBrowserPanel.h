#pragma once

#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/Resources/ResourceScan.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(ResourceBrowserPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;
    struct ResourceTypeDesc;

    // =============================================================================
    // ResourceBrowserPanel — the dockable "Resource Browser": the project's files on disk, in three
    //   views over ONE scanned tree.
    //
    //   It is a FILE browser, not a catalog (overview §3.5): nothing is loaded, resolved or
    //   reference-counted, and there is no manifest. What a file MEANS comes entirely from
    //   ResourceTypeRegistry — icon, label and double-click action, keyed by extension — so adding a
    //   type never touches this panel.
    //
    //   The tree is rebuilt only on Startup and on Refresh, never per frame: ImGui redraws constantly,
    //   the filesystem does not change constantly, and re-walking the disk at 60Hz would be both
    //   wasteful and jittery.
    //
    //   Like Hierarchy and Inspector it is a NATIVE panel with no privileges — EditorService registers
    //   it into the same PanelRegistry a game module registers into.
    // =============================================================================
    class ResourceBrowserPanel final : public IEditorPanel
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit ResourceBrowserPanel(EditorContext& InContext);
        ~ResourceBrowserPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
    public:
        ResourceBrowserPanel(const ResourceBrowserPanel&)            = delete;
        ResourceBrowserPanel& operator=(const ResourceBrowserPanel&) = delete;

        // =============================================================================
        // Types
        // =============================================================================
    private:
        /** Tiles = explorer grid (default), Tree = folder outline, List = flat. */
        enum class EBrowserView : Uint8 { Tiles, Tree, List };

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** Rescan every root from disk and log the outcome per root (the discrete, non-per-frame signal). */
        void Refresh();

        void DrawToolbar();
        void DrawBreadcrumb();
        void DrawTiles();
        void DrawTree();
        void DrawList();

        void DrawFolderNode(const ResourceFolder& InFolder, const ResourceRoot& InRoot, bool bIsRoot);
        void DrawListFolder(const ResourceFolder& InFolder, const ResourceRoot& InRoot, Uint64& InOutRowCount);
        void DrawFolderTile(const OpaaxString& InName, bool& bOutEnter);
        void DrawFileTile(const ResourceFile& InFile, const ResourceRoot& InRoot);
        void DrawFileRow(const ResourceFile& InFile, const ResourceRoot& InRoot, const char* InDisplayName);

        /**
         * Selection, tooltip and double-click activation for the LAST-SUBMITTED item — so one function
         * serves a tree Selectable and a tile InvisibleButton alike.
         */
        void ApplyFileBehavior(const ResourceFile& InFile, const ResourceRoot& InRoot, bool bClicked);

        /** @return The full display path of a file, "<Root>/<RelPath>" — its identity for selection + logs. */
        OpaaxString FullPathOf(const ResourceFile& InFile, const ResourceRoot& InRoot) const;

        const ResourceTypeDesc* FindType(const ResourceFile& InFile) const;
        bool                    MatchesFilter(const ResourceFile& InFile) const;

        /** @return true if this folder, or anything under it, has a file passing the filter — what lets Tree hide empty branches. */
        bool FolderHasMatch(const ResourceFolder& InFolder) const;

        /**
         * Walk m_TilePath to the folder it names, TRUNCATING it at the first segment that no longer
         * resolves (a folder deleted since the last scan must not strand the view).
         * @return nullptr at the Home level, where the roots themselves are the tiles.
         */
        const ResourceFolder* ResolveTileFolder();

        /** @return The root m_TilePath[0] names, or nullptr. */
        const ResourceRoot* ResolveTileRoot() const;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Builds the root list from the injected paths, then runs the first scan. */
        void            Startup()               override;

        /** Nothing the world's render depends on. */
        void            OnPreRender()           override {}

        /** Toolbar + breadcrumb pinned, the content area scrolling beneath them. */
        void            DrawContents()          override;

        /** No resource to release — the scanned tree is plain owned data. */
        void            Shutdown()              override {}

        PanelWindowStyle GetWindowStyle() const override { return { { 520.f, 320.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        // Project + Editor today. Data-driven on purpose: another root is one push_back in Startup.
        TDynArray<ResourceRoot> m_Roots;

        EBrowserView            m_View = EBrowserView::Tiles;

        // Tiles navigation: [] = Home (the roots), ["Project"], ["Project","Waves"], ...
        TDynArray<OpaaxString>  m_TilePath;

        OpaaxString             m_Filter;
        OpaaxString             m_SelectedPath;   // "<Root>/<RelPath>" — highlight only; NOT EditorSelection
    };
}
