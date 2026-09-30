#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/ResourceRef.hpp"   // claims on the sheet and textures
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/Undo/AnimationClipUndoables.h"          // open edit gestures

namespace Opaax
{
    OPAAX_LOG_CATEGORY(AnimationClipPanel);

    struct TextureResource;
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
    // AnimationClipPanel — the clip editor: step list, clip settings, and a playing preview.
    //   Opened by a double-click in the Resource Browser. The data is in EditorAnimationClipDocument;
    //   the panel only holds the image claims, the selection and the preview clock.
    //   The preview uses the panel's own clock (Edit worlds have no SpriteAnimationSubsystem).
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
        /** Name, dirty marker, Save, and clip-wide info. */
        void DrawHeader(const AnimationClipData& InData);

        /** The clip's own fields, with undo. */
        void DrawSettings(AnimationClipData& InData);

        /** Play / pause / scrub, and the image of the current step. */
        void DrawPreview(const AnimationClipData& InData);

        /** One row per step, with reorder and remove buttons. */
        void DrawStepList(const AnimationClipData& InData);

        /** The selected step's fields, and the sheet frame picker. */
        void DrawSelectedStep(AnimationClipData& InData);

        /** The sheet's frame names, as a combo. */
        void DrawFramePicker(const AnimationClipData& InData, Uint32 InStepIndex);

        /**
         * The image for InStepIndex: a crop of the sheet's texture, or the step's own texture.
         * @return An invalid image when there is nothing to show (the caller says so)
         */
        EditorImage ResolveStepImage(const AnimationClipData& InData, Uint32 InStepIndex);

        /** The clip's sheet, claimed once and released when the clip changes. Null when there is none. */
        const SpriteSheetData* ClaimSheet(const AnimationClipData& InData);

        /** A texture by asset path, claimed once. */
        const TextureResource* ClaimTexture(const OpaaxString& InAssetPath);

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

        /** Releases the claims while the ResourceManager and the GL context are alive. */
        void Shutdown()    override;

        PanelWindowStyle GetWindowStyle() const override { return { { 460.f, 560.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        /** The sheet the open clip names. */
        ResourceRef<SpriteSheetResource> m_SheetClaim;
        OpaaxString                      m_ClaimedSheetPath;

        /** One texture at a time: the sheet's image, or the previewed step's own. */
        ResourceRef<TextureResource> m_TextureClaim;
        OpaaxString                  m_ClaimedTexturePath;

        /** Highlighted step. -1 = none. */
        Int32 m_Selected = -1;

        // =============================================================================
        // The preview clock (the panel's own)
        // =============================================================================
        float m_PreviewTime    = 0.f;
        bool  m_bPreviewPlaying = true;

        /** The open undo gestures, one per group of fields. */
        ClipStepEdit     m_StepGesture;
        ClipSettingsEdit m_SettingsGesture;
        bool             m_bStepGestureOpen     = false;
        bool             m_bSettingsGestureOpen = false;
        bool             m_bWasStepItemActive     = false;
        bool             m_bWasSettingsItemActive = false;

        /** Largest preview edge, in pixels. */
        static constexpr float MAX_PREVIEW_SIZE = 180.f;
    };
}
