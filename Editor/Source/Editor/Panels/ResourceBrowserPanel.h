#pragma once

#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/ResourceRef.hpp"   // the icon cache holds Refs BY VALUE
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/Resources/ResourceScan.h"
#include "Editor/UI/IEditorUIBackend.h"                  // EditorImage — returned by value

namespace Opaax
{
    OPAAX_LOG_CATEGORY(ResourceBrowserPanel);

    struct ResourceFormatEntry;
    struct TextureResource;   // only NAMED by the icon cache — the RHI stays out of this header
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

        /** What the browser knows about one file: what it IS (engine) and how it looks (editor). */
        struct FileType
        {
            const ResourceFormatEntry* Format = nullptr;
            const ResourceTypeDesc*    Chrome = nullptr;
        };

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

        /**
         * Resolve a file in two steps: the ENGINE says which resource type owns its extension, the
         * EDITOR says how that type looks. Either half may be absent — an unregistered extension, or
         * a registered format no editor module gave chrome to.
         */
        FileType FindType(const ResourceFile& InFile) const;

        /** @return The chrome's override, else the format's own label, else "Unknown type". */
        static const char* LabelOf(const FileType& InType);

        /** @return The type's fallback text, else the unknown-type glyph. Drawn only when there is no icon image. */
        static const char* GlyphOf(const FileType& InType);

        /** Load every registered type's icon once, at Startup — the registry is sealed by then. */
        void LoadTypeIcons();

        /**
         * The type's icon image, from what LoadTypeIcons resolved.
         *
         * @return An INVALID image when the type registered none, when no editor space existed to
         *   resolve it against, or when the file was missing — all of which draw the glyph instead.
         */
        EditorImage IconOf(const FileType& InType) const;

        /**
         * What a TILE draws: the file's OWN image when it is already loaded, else its type's icon.
         *
         * Never loads anything — the thumbnail is a by-product of something else having loaded that
         * texture (this panel's Preview, or the game's own sprites), which is what keeps browsing a
         * folder of 500 images free.
         */
        EditorImage TileImageOf(const ResourceFile& InFile, const FileType& InType) const;

        /**
         * An editor-assets-relative icon path made absolute: the PROJECT's editor space first, then
         * the editor tool's own. Project-first is what lets a game override an editor icon.
         *
         * @return EMPTY when there is no editor space at all (no edited project).
         */
        OpaaxString ResolveIconPath(const OpaaxString& InIconRel) const;

        bool MatchesFilter(const ResourceFile& InFile) const;

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

        /** Release the icon claims. Panels shut down while the ResourceManager and the GL context
         *  are both still alive (LC3), which is what makes this the right place. */
        void            Shutdown()              override;

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

        // ResourceTypeID -> the claim keeping that type's icon loaded. Filled once at Startup from
        // the SEALED type registry, and an entry is kept even when the load failed, so the draw path
        // is a plain lookup with no I/O and no retry.
        TUnorderedMap<Uint32, ResourceRef<TextureResource>> m_TypeIcons;
    };
}
