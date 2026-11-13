#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/Events/Delegate.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Renderer/Textures/SpriteSheet.h"   // complete: a widget class visits its sheet reference
#include "Renderer/Textures/Texture2D.h"     // and its texture reference
#include "Resources/Asset/AssetRef.h"
#include "UI/UIButtonBase.h"
#include "UI/UIImageSource.h"
#include "UI/UIMargin.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    class ITexture2D;

    DECLARE_MULTICAST_DELEGATE(FOnUIClick)

    // =============================================================================
    // UIButton — a clickable image. A click is Down then Up still inside (a drag-off does not fire).
    //   Each state shows its own sheet frame (or Frame) tinted by its colour, sliced like a UIImage
    //   with the frame's border. Add a UIText child for a label.
    //   It handles its pointer events, so clicks do not reach the game.
    // =============================================================================
    class UIButton final : public UIButtonBase
    {
        OPAAX_OBJECT(UIButton, UIButtonBase)

        // =============================================================================
        // Authored state
        // =============================================================================
    public:
        LinearColor Normal   = { 0.25f, 0.25f, 0.28f, 1.f };
        LinearColor Hovered  = { 0.35f, 0.35f, 0.40f, 1.f };
        LinearColor Pressed  = { 0.15f, 0.15f, 0.18f, 1.f };
        LinearColor Disabled = { 0.20f, 0.20f, 0.20f, 0.5f };

        /** The button's art, tinted by the state colour. The sheet wins over the texture. */
        TAssetRef<Texture2D>   Texture;
        TAssetRef<SpriteSheet> Sheet;

        /** The sheet frame shown in Normal, and in any state whose own frame is -1. */
        Int32 Frame = -1;

        /** The sheet frames of the other states; -1 shows Frame. */
        Int32 HoveredFrame  = -1;
        Int32 PressedFrame  = -1;
        Int32 DisabledFrame = -1;

        /** 9-slice border in texture pixels. All zero = each state's frame's own border. */
        UIMargin Border;

        /** Canvas units a border pixel takes on screen: sizes the corners and edges, not what they show. */
        float BorderScale = 1.f;

        OPAAX_PROPERTIES(UIButton,
                         OPAAX_PROP(Normal),
                         OPAAX_PROP(Hovered),
                         OPAAX_PROP(Pressed),
                         OPAAX_PROP(Disabled),
                         OPAAX_PROP(Texture),
                         OPAAX_PROP(Sheet).SetTooltip("The art cut from a sheet. Wins over Texture when set."),
                         OPAAX_PROP(Frame).SetRange(-1.f, 4096.f).SetDragStep(1.f)
                             .SetTooltip("Shown in Normal, and in a state whose frame is -1.\n"
                                         "-1 is the sheet's own default frame."),
                         OPAAX_PROP(HoveredFrame).SetRange(-1.f, 4096.f).SetDragStep(1.f).SetTooltip("-1 shows Frame."),
                         OPAAX_PROP(PressedFrame).SetRange(-1.f, 4096.f).SetDragStep(1.f).SetTooltip("-1 shows Frame."),
                         OPAAX_PROP(DisabledFrame).SetRange(-1.f, 4096.f).SetDragStep(1.f).SetTooltip("-1 shows Frame."),
                         OPAAX_PROP(Border).SetDragStep(1.f)    // pixels
                             .SetTooltip("9-slice border in texture pixels. All zero: each frame's own."),
                         OPAAX_PROP(BorderScale).SetRange(0.01f, 16.f).SetDragStep(0.01f)
                             .SetTooltip("Canvas units a border pixel takes: 0.5 draws the corners\n"
                                         "half as big, showing the same art."))

        void        SetTexture(ITexture2D* InTexture);
        ITexture2D* GetTexture() const noexcept { return m_Texture; }

        /** The sheet frame InState shows: its own, or Frame when that is -1. */
        Int32 GetStateFrame(EUIButtonState InState) const noexcept;

        // =============================================================================
        // UIButtonBase
        // =============================================================================
    protected:
        void RebuildNormalState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;
        void RebuildHoveredState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;
        void RebuildPressedState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;
        void RebuildDisabledState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;

    private:
        /** Draws InFrame (sheet) or the texture, tinted InColor. False when named but not ready (re-armed). */
        bool DrawImage(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads, Int32 InFrame,
                       const LinearColor& InColor);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ITexture2D* m_Texture = nullptr;   // borrowed
        bool        m_bWarnedTiles = false;   // too many tiles, said once
    };
}
