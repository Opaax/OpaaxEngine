#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Assets/IAssetEditor.h"
#include "Editor/UI/IEditorUIBackend.h"   // EditorImage
#include "Resources/ResourceRef.hpp"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(AnimationClipEditor);

    class AnimationClip;
    struct AnimationStep;
    struct SpriteSheetData;
    struct SpriteSheetResource;
    struct TextureResource;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // AnimationClipEditor — a clip with a playing preview: Play, Pause, Restart and a scrub bar over
    //   the showing step's picture; then the clip's settings, the selected step's fields (its frame
    //   picked among the sheet's names) and the step list (add, remove, reorder; a texture dropped on
    //   a sheetless clip adds a step). The preview runs on its own clock: Edit worlds have no
    //   SpriteAnimationSubsystem.
    // =============================================================================
    class AnimationClipEditor final : public IAssetEditor
    {
    public:
        explicit AnimationClipEditor(EditorContext& InContext) : m_Context(InContext) {}

        /** Releases the claims (the window closes before the resources go). */
        ~AnimationClipEditor() override;

        //~Begin IAssetEditor interface
        void DrawContents(AssetDocument& InDocument) override;

        Vector2F GetDefaultSize() const override { return { 480.f, 680.f }; }
        //~End IAssetEditor interface

    private:
        /** Steps, ticks and length, and where the pictures come from. */
        void DrawSummary(const AnimationClip& InClip);

        /** Play / Pause / Restart, the scrub bar, and the showing step's picture. */
        void DrawPreview(const AnimationClip& InClip);

        /** Every property but the steps (the sheet, Fps, play mode). */
        void DrawSettings(AnimationClip& InClip);

        /** The selected step's fields: its frame for a sheet clip, its texture otherwise, and its hold. */
        void DrawSelectedStep(AnimationClip& InClip);

        /** The sheet's frame names, as a combo. */
        void DrawFramePicker(const AnimationClip& InClip, AnimationStep& InOutStep);

        /** Add / Remove / Up / Down, the texture drop target, and one row per step. */
        void DrawStepList(AnimationClip& InClip);

        /**
         * The picture of step InStepIndex: a crop of the sheet's texture, or the step's own texture.
         * @param OutPixelSize Its size in pixels, for the preview's proportions
         * @return An invalid image when there is nothing to show (the caller says so)
         */
        EditorImage ResolveStepImage(const AnimationClip& InClip, Uint32 InStepIndex, Vector2F& OutPixelSize);

        /** The clip's sheet, claimed while the clip names it. Null when there is none or it did not load. */
        const SpriteSheetData* ClaimSheet(const AnimationClip& InClip);

        /** A texture by asset path, claimed while this window is open. Null when it did not load. */
        const TextureResource* ClaimTexture(const OpaaxString& InAssetPath);

        EditorContext& m_Context;

        ResourceRef<SpriteSheetResource> m_SheetClaim;
        OpaaxString                      m_ClaimedSheetPath;

        /** The textures shown so far, by path id: a playing texture clip does not reload each step. */
        TUnorderedMap<Uint32, ResourceRef<TextureResource>> m_TextureClaims;

        /** The selected step, or -1. */
        Int32 m_Selected = -1;

        float m_PreviewTime     = 0.f;
        bool  m_bPreviewPlaying = true;

        /** Largest preview edge, in pixels. */
        static constexpr float MAX_PREVIEW_SIZE = 180.f;
    };
}
