#pragma once

#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/ResourceRef.hpp"   // one claim per open preview
#include "Editor/Panels/IEditorPanel.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(ResourcePreviewPanel);

    struct TextureResource;   // only NAMED by the held claims
}

namespace Opaax::Editor
{
    struct EditorContext;
    struct ResourcePreviewEntry;

    // =============================================================================
    // ResourcePreviewPanel — what a double-click on a resource opens, STACKED.
    //
    //   THE SEAM IS THE ONE THAT ALREADY EXISTED: ResourceTypeBuilder::SetActivate is "what does a
    //   double-click do", and Map and Level have used it since M2d. A texture opening a viewer is
    //   the same shape, which is why the Inspector's TPropertyDrawer contract did NOT have to grow
    //   an EditorContext to get a preview — the preview simply lives where a context already is
    //   (I15 untouched).
    //
    //   It holds MANY previews, each in its own collapsible section with a close button, because
    //   comparing two images means having both on screen. ResourcePreview owns the list; this panel
    //   owns one CLAIM per entry — whoever draws the pixels keeps them alive.
    //
    //   It draws the identity of ANY resource (name, path, type) and the CONTENT of the ones it
    //   knows how to show — today exactly one, TextureResource. That single branch is the honest
    //   shape while there is one previewable type; the growth point is named in the .cpp.
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
        /** One entry's section: header, identity, content. @return true to close it. */
        bool DrawEntry(const ResourcePreviewEntry& InEntry, Uint64 InIndex, bool bInFocused);

        /** Name / Path / Type — everything true of a resource whatever its type. */
        void DrawIdentity(const ResourcePreviewEntry& InEntry) const;

        /** The image itself, aspect-fit into a square box, plus its dimensions. */
        void DrawTexture(const OpaaxString& InAbsPath);

        /**
         * The claim on InAbsPath, loading it on first request and keeping it.
         *
         * Keyed by INTERNED path so a redraw is an integer lookup, never a Load — a Load of a
         * resident path is only a dedup hit, but one that bumps a refcount once per frame.
         */
        const TextureResource* ClaimTexture(const OpaaxString& InAbsPath);

        /** Drop claims for paths no longer open, so closing a preview actually releases its image. */
        void ReleaseClosedClaims();

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire — a preview exists only once something is asked for. */
        void Startup()     override {}

        /** Nothing the world's render depends on. */
        void OnPreRender() override {}

        void DrawContents() override;

        /** Release every claim while the ResourceManager and the GL context are both alive (LC3). */
        void Shutdown()    override;

        PanelWindowStyle GetWindowStyle() const override { return { { 320.f, 420.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        /** The largest edge a preview image is drawn at, in pixels. */
        static constexpr float MAX_IMAGE_SIZE = 256.f;

        // Interned abs path -> the claim keeping that image loaded. One per OPEN entry; pruned by
        // ReleaseClosedClaims so a closed preview does not keep its texture resident forever.
        TUnorderedMap<Uint32, ResourceRef<TextureResource>> m_Claims;
    };
}
