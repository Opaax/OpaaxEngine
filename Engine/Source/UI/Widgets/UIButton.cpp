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

    void UIButton::SetStyle(const OpaaxString& InStylePath)
    {
        Style.Path = InStylePath;
        InvalidateContent();
    }

    const UIStateBrushes& UIButton::GetBrushes() const
    {
        const UICanvas* lCanvas = GetCanvas();
        if (Style.IsEmpty() || lCanvas == nullptr || lCanvas->GetAssets() == nullptr)
        {
            return Brushes;   // none named, or not drawn yet: no way to load it
        }

        // Read again each time: a saved style reloads in place.
        const UIButtonStyle* lStyle = Cast<UIButtonStyle>(lCanvas->GetAssets()->ResolveAsset(Style.Path.CStr()));
        if (lStyle == nullptr)
        {
            if (m_WarnedStyle != Style.Path)
            {
                m_WarnedStyle = Style.Path;
                OPAAX_LOG(LogUIBrush, Warn, "Button style '{}' did not load as a UI Button Style: '{}' draws its own "
                          "Brushes", Style.Path.CStr(), Name.IsEmpty() ? "a button" : Name.CStr());
            }
            return Brushes;
        }
        return lStyle->Brushes;
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
        m_WarnedStyle = OpaaxString();
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
        const UIStateBrushes& lBrushes = GetBrushes();
        if (lFadeBefore < lBrushes.FadeDuration)
        {
            InvalidateContent();
            bStay = m_FadeSeconds < lBrushes.FadeDuration;
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
        const UIStateBrushes& lBrushes = GetBrushes();
        UIBrush               lBrush   = lBrushes.For(GetInteractionState());

        // The first draw, and a canvas nobody ticks (the editor's document), show the tint at once.
        const UICanvas* lCanvas = GetCanvas();
        if (!m_bDrawnOnce || lCanvas == nullptr || !lCanvas->IsTicked())
        {
            m_FadeSeconds = lBrushes.FadeDuration;
        }
        lBrush.Tint  = FadeTint(m_FadeFrom, lBrush.Tint, m_FadeSeconds, lBrushes.FadeDuration, lBrushes.FadeEase);
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
