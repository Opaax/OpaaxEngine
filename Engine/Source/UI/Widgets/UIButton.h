#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/OpaaxTypes.h"
#include "UI/Styles/UIButtonStyle.h"   // complete: a widget class visits its style reference
#include "UI/UIBrush.h"
#include "UI/UIButtonBase.h"
#include "UI/UIStateBrushes.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    class ITexture2D;

    // =============================================================================
    // UIButton — a clickable image. A click is Down then Up still inside (a drag-off does not fire).
    //   Each interaction state draws its brush (UIStateBrushes), its Style's when it names one that
    //   loads: the image swaps at once, the tint fades over FadeDuration, and a state's clip starts over
    //   when the state is entered. Add a UIText child for a label. It handles its pointer events, so
    //   clicks do not reach the game.
    // =============================================================================
    class UIButton final : public UIButtonBase
    {
        OPAAX_OBJECT(UIButton, UIButtonBase)

        // =============================================================================
        // Authored state
        // =============================================================================
    public:
        /** A UI Button Style asset: when it loads, its brushes are drawn instead of Brushes. */
        TAssetRef<UIButtonStyle> Style;

        /** What each state draws, and the tint's fade between them, when no Style is in use. */
        UIStateBrushes Brushes;

        OPAAX_PROPERTIES(UIButton,
                         OPAAX_PROP(Style).SetTooltip("A UI Button Style asset, shared by many buttons. When set,\n"
                                                      "its brushes are drawn and Brushes below is not."),
                         OPAAX_PROP(Brushes).SetTooltip("This button's own brushes: drawn when it names no Style,\n"
                                                        "or when that style does not load."))

        /** Style.Path; the button draws again. */
        void SetStyle(const OpaaxString& InStylePath);

        /**
         * The brushes in use: the Style's when it loads through the canvas, else Brushes (said once in the
         * log when a named style does not load).
         */
        const UIStateBrushes& GetBrushes() const;

        /** A runtime texture, borrowed: every state shows it, in its own tint. */
        void        SetTexture(ITexture2D* InTexture);
        ITexture2D* GetTexture() const noexcept { return m_Texture; }

        /** What InState draws (UIStateBrushes::For on the brushes in use). */
        UIBrush GetStateBrush(EUIInteractionState InState) const { return GetBrushes().For(InState); }

        /** The tint drawn now, partway through a fade. */
        const LinearColor& GetShownTint() const noexcept { return m_ShownTint; }

        // =============================================================================
        // UIWidget
        // =============================================================================
    public:
        OpaaxString DescribeBrush() const override;
        void        PostEditChange() override;

    protected:
        void Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;
        bool Tick(float InSeconds) override;
        void OnReset() override;
        void OnInteractionStateChanged(EUIInteractionState InFrom, EUIInteractionState InTo) override;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ITexture2D* m_Texture      = nullptr;   // borrowed
        bool        m_bWarnedTiles = false;     // too many tiles, said once
        bool        m_bWarnedClip  = false;     // a clip problem, said once
        mutable OpaaxString m_WarnedStyle;      // the style path last said not to load

        /** Seconds since the state was entered (its clip's time). */
        float m_StateSeconds = 0.f;
        bool  m_bDrawnOnce   = false;   // the first draw jumps to its tint

        /** The tint's fade: from the one shown when the state changed. */
        LinearColor m_FadeFrom;
        LinearColor m_ShownTint;
        float       m_FadeSeconds = 0.f;

        UIBrushClip m_DrawnClip;   // where the state's clip was when last drawn; its Clip only tested for null later
    };
}
