#pragma once

#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"
#include "Resources/Asset/AssetHandle.h"
#include "Editor/Extensions/AssetTypeRegistry.h"   // FAssetActivate
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/Resources/ResourceScan.h"
#include "Editor/UI/IEditorUIBackend.h"                  // EditorImage

namespace Opaax
{
    OPAAX_LOG_CATEGORY(ResourceBrowserPanel);

    struct ResourceFormatEntry;
    class Texture2D;
}

namespace Opaax::Editor
{
    struct EditorContext;
    struct ResourceSavedEvent;

    // =============================================================================
    // ResourceBrowserPanel — the "Resource Browser": the project's files on disk, in three views
    //   (tiles, tree, list) over one scanned tree. A file browser: nothing is loaded. A file's icon,
    //   label and double-click action come from ResourceTypeRegistry.
    //   Rescanned only on Startup and Refresh (not every frame).
    // =============================================================================
    class ResourceBrowserPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Resource Browser);
        
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
        /** Tiles = grid (default), Tree = folder outline, List = flat. */
        enum class EBrowserView : Uint8 { Tiles, Tree, List };

        /** How one asset class looks, built once at Startup from AssetTypes(). */
        struct AssetChrome
        {
            OpaaxString             Label;
            OpaaxString             Glyph;
            TAssetHandle<Texture2D> Icon;

            /** What a double-click does; empty opens the asset's window. */
            FAssetActivate          Activate;
        };

        /** One file: an asset (its resource format, its class's look), or neither for any other file. */
        struct FileType
        {
            const ResourceFormatEntry* Format = nullptr;
            const AssetChrome*         Asset  = nullptr;
        };

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** Rescans every root and logs the result per root. */
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
         * Selection, tooltip and double-click for the last submitted item (tree row or tile).
         */
        void ApplyFileBehavior(const ResourceFile& InFile, const ResourceRoot& InRoot, bool bClicked);

        /** @return The file's display path, "<Root>/<RelPath>" */
        OpaaxString FullPathOf(const ResourceFile& InFile, const ResourceRoot& InRoot) const;

        /**
         * Resolves a file: the engine gives the resource type of its extension, the editor gives how the
         * type looks. Either may be missing.
         */
        FileType FindType(const ResourceFile& InFile) const;

        /**
         * What an asset file holds, for its drag payload: its class then each parent, by name id (read at
         * scan time).
         * @return Empty for other files, and for a class no module registered
         */
        TDynArray<Uint32> KindsOf(const ResourceFile& InFile) const;

        /** @return The chrome's label, else the format's label, else "Unknown type" */
        static const char* LabelOf(const FileType& InType);

        /** @return The type's fallback text, else the unknown-type glyph (used when there is no icon image) */
        static const char* GlyphOf(const FileType& InType);

        /** Builds each asset class's look once, at Startup, its icon loaded. */
        void LoadTypeIcons();

        /**
         * The New Asset menu: the classes an asset can be made of, by category. Picking one writes a new
         * asset in InFolderAbsPath and opens it.
         */
        void DrawNewAssetMenu(const OpaaxString& InFolderAbsPath);

        /** A document wrote a file: rescans when it is one the browser does not list yet. */
        void HandleResourceSaved(const ResourceSavedEvent& InEvent);

        /** @return True if the scanned roots list InAbsPath */
        bool IsListed(const OpaaxString& InAbsPath) const;

        /**
         * The type's icon image.
         * @return An invalid image if the type has none, there is no editor space, or the file is missing
         *   (the glyph is drawn instead)
         */
        EditorImage IconOf(const FileType& InType) const;

        /**
         * What a tile shows: the file's own image if already loaded, else its type's icon. Never loads.
         */
        EditorImage TileImageOf(const ResourceFile& InFile, const FileType& InType) const;

        /**
         * An icon path made absolute: the project's editor space first, then the editor's own
         * (so a game can override an icon).
         * @return Empty without an editor space
         */
        OpaaxString ResolveIconPath(const OpaaxString& InIconRel) const;

        /** The image at InAbsPath, loaded and kept. Empty for an empty path. */
        TAssetHandle<Texture2D> LoadIcon(const OpaaxString& InAbsPath) const;

        bool MatchesFilter(const ResourceFile& InFile) const;

        /** @return True if this folder or anything under it has a file passing the filter */
        bool FolderHasMatch(const ResourceFolder& InFolder) const;

        /**
         * The folder m_TilePath names, truncating the path at the first missing segment.
         * @return nullptr at Home (the roots are the tiles)
         */
        const ResourceFolder* ResolveTileFolder();

        /** @return The root m_TilePath[0] names, or nullptr */
        const ResourceRoot* ResolveTileRoot() const;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Builds the root list, then scans. */
        void            Startup()               override;

        /** Nothing the world render depends on. */
        void            OnPreRender()           override {}

        /** Toolbar and breadcrumb pinned, content scrolling below. */
        void            DrawContents()          override;

        /** Releases the icon claims (the ResourceManager and GL context are still alive). */
        void            Shutdown()              override;

        PanelWindowStyle GetWindowStyle() const override { return { { 520.f, 320.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        // Project and Editor roots.
        TDynArray<ResourceRoot> m_Roots;

        EBrowserView            m_View = EBrowserView::Tiles;

        // Tiles navigation: [] = Home (the roots), ["Project"], ["Project","Waves"], ...
        TDynArray<OpaaxString>  m_TilePath;

        OpaaxString             m_Filter;
        OpaaxString             m_SelectedPath;   // "<Root>/<RelPath>", highlight only (not EditorSelection)

        // An asset class's name id -> its look. Built once at Startup.
        TUnorderedMap<Uint32, AssetChrome> m_AssetChrome;
    };
}
