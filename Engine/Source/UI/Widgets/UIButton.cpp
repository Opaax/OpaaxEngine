#include "UI/Widgets/UIButton.h"

#include "UI/UICanvas.h"
#include "UI/UISlice.h"

namespace Opaax
{
    void UIButton::SetTexture(ITexture2D* InTexture)
    {
        m_Texture = InTexture;
        InvalidateContent();
    }

    OpaaxString UIButton::DescribeBrush() const
    {
        return Opaax::DescribeBrush(GetStateBrush(GetInteractionState()), m_Texture != nullptr, m_DrawnClip);
    }

    void UIButton::PostEditChange()
    {
        UIButtonBase::PostEditChange();
        Brushes.Sanitize();
        m_bWarnedClip = false;
        InvalidateContent();
    }

    void UIButton::OnInteractionStateChanged(EUIInteractionState, EUIInteractionState)
    {
        // The tint fades from the one on screen; the new state's clip starts over. The image swaps at once.
        m_FadeFrom     = m_ShownTint;
        m_FadeSeconds  = 0.f;
        m_StateSeconds = 0.f;
        m_DrawnClip    = {};
        InvalidateContent();
        WakeUp();
    }

    bool UIButton::Tick(const float InSeconds)
    {
        const float lFadeBefore = m_FadeSeconds;
        m_FadeSeconds  += InSeconds;
        m_StateSeconds += InSeconds;

        bool bStay = false;

        // Redrawn every frame of the fade, and once past its end so the last tint is exact.
        if (lFadeBefore < Brushes.FadeDuration)
        {
            InvalidateContent();
            bStay = m_FadeSeconds < Brushes.FadeDuration;
        }

        UICanvas* const   lCanvas = GetCanvas();
        const UIBrushClip lClip   = SampleBrushClip(lCanvas != nullptr ? lCanvas->GetAssets() : nullptr,
                                                    GetStateBrush(GetInteractionState()), m_StateSeconds);
        if (lClip.Clip != nullptr)
        {
            if (lClip.Step != m_DrawnClip.Step || m_DrawnClip.Clip == nullptr)
            {
                InvalidateContent();
            }
            bStay = bStay || !lClip.bFinished;
        }
        return bStay;
    }

    void UIButton::OnReset()
    {
        m_StateSeconds = 0.f;
        m_FadeSeconds  = 0.f;
        m_bDrawnOnce   = false;
        m_DrawnClip    = {};
        InvalidateContent();
    }

    void UIButton::Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        UIBrush lBrush = GetStateBrush(GetInteractionState());

        // The first draw, and a canvas nobody ticks (the editor's document), show the tint at once.
        const UICanvas* lCanvas = GetCanvas();
        if (!m_bDrawnOnce || lCanvas == nullptr || !lCanvas->IsTicked())
        {
            m_FadeSeconds = Brushes.FadeDuration;
        }
        lBrush.Tint  = FadeTint(m_FadeFrom, lBrush.Tint, m_FadeSeconds, Brushes.FadeDuration, Brushes.FadeEase);
        m_ShownTint  = lBrush.Tint;
        m_bDrawnOnce = true;

        // Same rule as UIImage: named but not ready draws nothing and retries; nothing named is a plain quad.
        const EUIBrushDraw lDraw = DrawBrush(InContext, GetBounds(), lBrush, m_Texture, m_StateSeconds, OutQuads,
                                             &m_DrawnClip);

        if (m_DrawnClip.Issue != EUIBrushIssue::None && !m_bWarnedClip)
        {
            m_bWarnedClip = true;
            OPAAX_LOG(LogUIBrush, Warn, "'{}': {}", Name.CStr(), DescribeBrushIssue(lBrush, m_DrawnClip).CStr());
        }

        // A state's clip that can still change keeps me ticking (only a ticked canvas runs it).
        if (m_DrawnClip.Clip != nullptr && !m_DrawnClip.bFinished && !IsAwake())
        {
            WakeUp();
        }

        if (lDraw == EUIBrushDraw::NotReady)
        {
            InvalidateContent();
            return;
        }

        if (lDraw == EUIBrushDraw::Stretched && !m_bWarnedTiles)
        {
            m_bWarnedTiles = true;
            OPAAX_LOG(LogUISlice, Warn, "'{}' would take more than {} tiles: stretched instead (raise BorderScale)",
                      Name.CStr(), UI_MAX_TILED_QUADS);
        }
    }
}
