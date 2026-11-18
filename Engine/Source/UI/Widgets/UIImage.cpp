#include "UI/Widgets/UIImage.h"

#include "Core/Maths/Maths.h"

#include "Core/Reflection/OpaaxEnumJson.h"
#include "UI/UIBinding.h"
#include "UI/UIBrush.h"
#include "UI/UICanvas.h"
#include "UI/UISlice.h"

namespace Opaax
{
    namespace
    {
        /** The rect left by a fill: cropped from its min edge on the filled axis. */
        Bounds2D FillClip(const Bounds2D& InRect, const EUIFill InFill, const float InAmount) noexcept
        {
            const Vector2F lMin  = InRect.Min();
            const Vector2F lMax  = InRect.Max();
            const Vector2F lSize = InRect.Size();

            if (InFill == EUIFill::Horizontal)
            {
                return Bounds2D::FromMinMax(lMin, { lMin.x + lSize.x * InAmount, lMax.y });
            }

            return Bounds2D::FromMinMax(lMin, { lMax.x, lMin.y + lSize.y * InAmount });
        }
    }

    void UIImage::SetColor(const LinearColor& InColor)
    {
        Brush.Tint = InColor;
        InvalidateContent();
    }

    void UIImage::SetAnimation(const OpaaxString& InClipPath)
    {
        Brush.Animation.Path = InClipPath;
        m_ClipSeconds        = 0.f;
        m_bWarnedClip        = false;
        InvalidateContent();
    }

    void UIImage::SetStyle(const OpaaxString& InStylePath)
    {
        Style.Path    = InStylePath;
        m_ClipSeconds = 0.f;
        InvalidateContent();
    }

    UIBrush UIImage::GetDrawnBrush() const
    {
        const UICanvas* lCanvas = GetCanvas();
        if (Style.IsEmpty() || lCanvas == nullptr || lCanvas->GetAssets() == nullptr)
        {
            return Brush;
        }

        const UIButtonStyle* lStyle = ResolveButtonStyle(lCanvas->GetAssets(), Style);
        if (lStyle == nullptr)
        {
            if (m_WarnedStyle != Style.Path)
            {
                m_WarnedStyle = Style.Path;
                OPAAX_LOG(LogUIBrush, Warn, "Style '{}' did not load as a UI Button Style: '{}' draws its own Brush",
                          Style.Path.CStr(), Name.IsEmpty() ? "an image" : Name.CStr());
            }
            return Brush;
        }
        return lStyle->Brushes.For(GetInteractionState());
    }

    void UIImage::SetFill(const EUIFill InFill, const float InAmount)
    {
        Fill       = InFill;
        FillAmount = InAmount;
        InvalidateContent();
    }

    void UIImage::SetFillAmount(const float InAmount)
    {
        FillAmount = InAmount;
        InvalidateContent();
    }

    void UIImage::SetTexturePath(const OpaaxString& InAssetPath)
    {
        Brush.Texture.Path = InAssetPath;
        InvalidateContent();
    }

    void UIImage::SetSheetFrame(const OpaaxString& InSheetPath, const Int32 InFrame)
    {
        Brush.Sheet.Path = InSheetPath;
        Brush.Frame      = InFrame;
        InvalidateContent();
    }

    void UIImage::SetBorder(const UIMargin& InBorder)
    {
        Brush.Border = InBorder;
        InvalidateContent();
    }

    void UIImage::SetBorderScale(const float InBorderScale)
    {
        Brush.BorderScale = InBorderScale;
        InvalidateContent();
    }

    void UIImage::SetTexture(ITexture2D* InTexture)
    {
        m_Texture = InTexture;
        InvalidateContent();
    }

    void UIImage::OnPullBindings(UIBindingTable& InBindings)
    {
        if (FillBinding.IsEmpty())
        {
            return;
        }

        UIBoundValue lValue;
        if (!InBindings.Read(FillBinding, lValue))
        {
            return;
        }

        if (!m_bFillBound)
        {
            m_bFillBound = true;
            OPAAX_LOG(LogUIBinding, Trace, "'{}' bound '{}' — fill {}", Name.CStr(), FillBinding.CStr(), lValue.ToFloat());
        }

        if (lValue.ToFloat() != FillAmount)
        {
            SetFillAmount(lValue.ToFloat());
        }
    }

    void UIImage::Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        const float lAmount = Fill == EUIFill::None ? 1.f : Maths::Clamp(FillAmount, 0.f, 1.f);
        if (lAmount <= 0.f)
        {
            return;
        }

        // Runtime pointer > clip > sheet frame > texture. Named but not ready: draw nothing and retry;
        // nothing named: a plain colour.
        const UIBrush      lBrush = GetDrawnBrush();
        const EUIBrushDraw lDraw  = DrawBrush(InContext, GetBounds(), lBrush, m_Texture, m_ClipSeconds, OutQuads, &m_DrawnClip);

        if (m_DrawnClip.Issue != EUIBrushIssue::None && !m_bWarnedClip)
        {
            m_bWarnedClip = true;
            OPAAX_LOG(LogUIBrush, Warn, "'{}': {}", Name.CStr(), DescribeBrushIssue(lBrush, m_DrawnClip).CStr());
        }

        // A clip that can still change keeps me ticking (only a ticked canvas runs it).
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

        // The fill clips what was emitted, from the min edge, with its UVs (the texture is cut,
        // not squashed), for 9 quads as for 1.
        if (Fill != EUIFill::None && lAmount < 1.f)
        {
            ClipQuadsTo(OutQuads, FillClip(GetBounds(), Fill, lAmount));
        }
    }

    bool UIImage::Tick(const float InSeconds)
    {
        m_ClipSeconds += InSeconds;

        UICanvas* const   lCanvas = GetCanvas();
        const UIBrushClip lClip   = SampleBrushClip(lCanvas != nullptr ? lCanvas->GetAssets() : nullptr, GetDrawnBrush(),
                                                    m_ClipSeconds);
        if (lClip.Clip == nullptr)
        {
            return false;   // no clip any more: nothing to play
        }

        // Rebuilt only when the step shown changes.
        if (lClip.Step != m_DrawnClip.Step || m_DrawnClip.Clip == nullptr)
        {
            InvalidateContent();
        }
        return !lClip.bFinished;
    }

    void UIImage::OnReset()
    {
        m_ClipSeconds = 0.f;
        m_DrawnClip   = {};
        InvalidateContent();
    }

    void UIImage::OnInteractionStateChanged(EUIInteractionState, EUIInteractionState)
    {
        // Only a style draws per state: its new state's art at once, its clip from the first step.
        if (!Style.IsEmpty())
        {
            m_ClipSeconds = 0.f;
            m_DrawnClip   = {};
            InvalidateContent();
        }
    }

    OpaaxString UIImage::DescribeBrush() const
    {
        return Opaax::DescribeBrush(GetDrawnBrush(), m_Texture != nullptr, m_DrawnClip);
    }
}
