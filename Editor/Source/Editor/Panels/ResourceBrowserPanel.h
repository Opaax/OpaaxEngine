#pragma once

#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/ResourceRef.hpp"
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/Resources/ResourceScan.h"
#include "Editor/UI/IEditorUIBackend.h"                  // EditorImage

namespace Opaax
{
    OPAAX_LOG_CATEGORY(ResourceBrowserPanel);

    struct ResourceFormatEntry;
    struct TextureResource;
}

namespace Opaax::Editor
{
    struct EditorContext;
    struct ResourceTypeDesc;

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

        /** One file: its type (engine) and how it looks (editor). */
        struct FileType
        {
            const ResourceFormatEntry* Format = nullptr;
            const ResourceTypeDesc*    Chrome = nullptr;
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

        /** @return The chrome's label, else the format's label, else "Unknown type" */
        static const char* LabelOf(const FileType& InType);

        /** @return The type's fallback text, else the unknown-type glyph (used when there is no icon image) */
        static const char* GlyphOf(const FileType& InType);

        /** Loads every registered type's icon once, at Startup. */
        void LoadTypeIcons();

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

        // ResourceTypeID -> the icon texture claim. Filled once at Startup (failed loads kept too,
        // so drawing is a plain lookup).
        TUnorderedMap<Uint32, ResourceRef<TextureResource>> m_TypeIcons;
    };
}
