#pragma once

#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/Resources/ResourcePreviewClaim.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(ResourcePreviewPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;
    struct ResourcePreviewEntry;

    // =============================================================================
    // ResourcePreviewPanel — what a double-click on a resource opens (through
    //   ResourceTypeBuilder::SetActivate, like maps and levels). Holds several previews, each in its
    //   own collapsible section (to compare two images). ResourcePreview owns the list; this panel owns
    //   one live preview per entry. Shows every resource's name, path and type, and the content of the
    //   ones that have a preview.
    // =============================================================================
    class ResourcePreviewPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Resource Preview);
        
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit ResourcePreviewPanel(EditorContext& InContext);
        ~ResourcePreviewPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
        ResourcePreviewPanel(const ResourcePreviewPanel&)            = delete;
        ResourcePreviewPanel& operator=(const ResourcePreviewPanel&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** One entry's section. @return True to close it */
        bool DrawEntry(const ResourcePreviewEntry& InEntry, Uint64 InIndex, bool bInFocused);

        /** Name / Path / Type. */
        void DrawIdentity(const ResourcePreviewEntry& InEntry) const;

        /**
         * The live preview for InEntry, built from its type's chrome on first request and kept
         * (keyed by path id, so a redraw is a lookup).
         * @return nullptr if the type has no preview, or the file did not load
         */
        IResourcePreviewClaim* ClaimFor(const ResourcePreviewEntry& InEntry);

        /** Drops the previews of closed entries (releases their resources). */
        void ReleaseClosedClaims();

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire. */
        void Startup()     override {}

        /** Nothing the world render depends on. */
        void OnPreRender() override {}

        void DrawContents() override;

        /** Releases every claim (the ResourceManager and GL context are still alive). */
        void Shutdown()    override;

        PanelWindowStyle GetWindowStyle() const override { return { { 320.f, 420.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        // Path id -> live preview (claim and drawer), one per open entry. Type-erased, so any
        // previewable type works.
        TUnorderedMap<Uint32, TUniquePtr<IResourcePreviewClaim>> m_Claims;
    };
}
