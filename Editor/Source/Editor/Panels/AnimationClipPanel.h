#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/ResourceRef.hpp"   // the claim on the clip's sheet + textures
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/Undo/AnimationClipUndoables.h"          // the open edit gestures

namespace Opaax
{
    OPAAX_LOG_CATEGORY(AnimationClipPanel);

    struct TextureResource;      // only NAMED by the held claims
    struct SpriteSheetResource;
    struct AnimationClipData;
    struct SpriteSheetData;
    struct SpriteFrame;
}

namespace Opaax::Editor
{
    struct EditorContext;
    struct EditorImage;

    // =============================================================================
    // AnimationClipPanel — the clip EDITOR: the step list, the clip's settings, and a preview that
    //   actually plays.
    //
    //   Opened by a double-click in the Resource Browser, through the seam that already answers
    //   "what does a double-click do" (ResourceTypeBuilder::SetActivate) — the same route Map,
    //   Level, the texture preview and the sheet editor use, so this needed no new plumbing.
    //
    //   It draws EditorAnimationClipDocument, which owns the data; the panel owns only the CLAIMS
    //   on the images, the selection and the preview clock, all of which are presentation. That
    //   split is what lets a Save command and an undo step write the clip without going through a
    //   panel.
    //
    //   THE PREVIEW RUNS ON THE PANEL'S OWN CLOCK, not the world's. An Edit world has no
    //   SpriteAnimationSubsystem by construction (it is Play-only), so authoring playback cannot
    //   borrow one — and must not, because the thing being previewed is the DOCUMENT's copy, which
    //   the running game has never seen.
    // =============================================================================
    class AnimationClipPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Animation Clip);

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit AnimationClipPanel(EditorContext& InContext);
        ~AnimationClipPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
        AnimationClipPanel(const AnimationClipPanel&)            = delete;
        AnimationClipPanel& operator=(const AnimationClipPanel&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** Name, dirty marker, Save, and what is true of the clip as a whole. */
        void DrawHeader(const AnimationClipData& InData);

        /** The clip's own fields, bracketed for undo the way the Inspector brackets its drawers. */
        void DrawSettings(AnimationClipData& InData);

        /** Play / pause / scrub, and the picture of whatever step the clock is on. */
        void DrawPreview(const AnimationClipData& InData);

        /** One selectable row per step, with the buttons that reorder and remove it. */
        void DrawStepList(const AnimationClipData& InData);

        /** The selected step's fields, plus the picker that names a sheet frame. */
        void DrawSelectedStep(AnimationClipData& InData);

        /** The sheet's frame names as a combo — the reason a frame is worth naming at all. */
        void DrawFramePicker(const AnimationClipData& InData, Uint32 InStepIndex);

        /**
         * The image for InStepIndex: a crop of the sheet's texture, or the step's own texture.
         *
         * @return An invalid image when there is nothing to show, which the caller says out loud
         *   rather than drawing a blank rectangle ([[L15]]).
         */
        EditorImage ResolveStepImage(const AnimationClipData& InData, Uint32 InStepIndex);

        /** The clip's sheet, claimed once and dropped when the clip changes. Null when it names none. */
        const SpriteSheetData* ClaimSheet(const AnimationClipData& InData);

        /** A texture by asset path, claimed once. Serves the sheet's image and a step's own alike. */
        const TextureResource* ClaimTexture(const OpaaxString& InAssetPath);

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire — an image exists only once a clip is opened. */
        void Startup()     override {}

        /** Nothing the world's render depends on. */
        void OnPreRender() override {}

        void DrawContents() override;

        /** Release the claims while the ResourceManager and the GL context are both alive (LC3). */
        void Shutdown()    override;

        PanelWindowStyle GetWindowStyle() const override { return { { 460.f, 560.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        /** The sheet the open clip names, keyed so a re-read of the same one is free. */
        ResourceRef<SpriteSheetResource> m_SheetClaim;
        OpaaxString                      m_ClaimedSheetPath;

        /** One texture at a time: the sheet's image, or the previewed step's own. */
        ResourceRef<TextureResource> m_TextureClaim;
        OpaaxString                  m_ClaimedTexturePath;

        /** Which step the list highlights. -1 = none. Presentation, not document state. */
        Int32 m_Selected = -1;

        // =============================================================================
        // The preview clock — the panel's own, never the world's
        // =============================================================================
        float m_PreviewTime    = 0.f;
        bool  m_bPreviewPlaying = true;

        /** The open edit gestures, one per group of fields, for the Inspector's reason. */
        ClipStepEdit     m_StepGesture;
        ClipSettingsEdit m_SettingsGesture;
        bool             m_bStepGestureOpen     = false;
        bool             m_bSettingsGestureOpen = false;
        bool             m_bWasStepItemActive     = false;
        bool             m_bWasSettingsItemActive = false;

        /** The largest edge the preview is drawn at, in pixels. */
        static constexpr float MAX_PREVIEW_SIZE = 180.f;
    };
}
