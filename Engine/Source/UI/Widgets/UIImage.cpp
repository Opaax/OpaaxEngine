#include "UI/Widgets/UIImage.h"

#include <algorithm>

#include "Core/Reflection/OpaaxEnumJson.h"
#include "UI/UIBinding.h"
#include "UI/UIImageSource.h"
#include "UI/UISlice.h"

namespace Opaax
{
    namespace
    {
        /** What a fill leaves standing: the rect cropped from its min edge on the filled axis. */
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
        Color = InColor;
        InvalidateContent();
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
        Texture.Path = InAssetPath;
        InvalidateContent();
    }

    void UIImage::SetSheetFrame(const OpaaxString& InSheetPath, const Int32 InFrame)
    {
        Sheet.Path = InSheetPath;
        Frame      = InFrame;
        InvalidateContent();
    }

    void UIImage::SetBorder(const UIMargin& InBorder)
    {
        Border = InBorder;
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

    void UIImage::SaveFields(nlohmann::json& InOutJson) const
    {
        UIWidget::SaveFields(InOutJson);

        // The PATH is written; the runtime pointer beside it is not, because it is a borrowed
        // handle that only code can hand over (**UI17**).
        InOutJson["Color"]       = Color;
        InOutJson["Texture"]     = Texture;
        InOutJson["Sheet"]       = Sheet;
        InOutJson["Frame"]       = Frame;
        InOutJson["Border"]      = Border;
        InOutJson["Fill"]        = Fill;
        InOutJson["FillAmount"]  = FillAmount;
        InOutJson["FillBinding"] = FillBinding;
    }

    void UIImage::LoadFields(const nlohmann::json& InJson)
    {
        UIWidget::LoadFields(InJson);

        Color       = InJson.value("Color", Color);
        Texture     = InJson.value("Texture", Texture);
        Sheet       = InJson.value("Sheet", Sheet);
        Frame       = InJson.value("Frame", Frame);
        Border      = InJson.value("Border", Border);
        Fill        = InJson.value("Fill", Fill);
        FillAmount  = InJson.value("FillAmount", FillAmount);
        FillBinding = InJson.value("FillBinding", FillBinding);
    }

    void UIImage::Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        const float lAmount = Fill == EUIFill::None ? 1.f : std::clamp(FillAmount, 0.f, 1.f);
        if (lAmount <= 0.f)
        {
            return;
        }

        // Runtime pointer > sheet frame > texture (UI25). Something named but not ready draws
        // nothing and asks again next frame (**UI3**); nothing named is a plain colour.
        UIResolvedImage lImage;
        bool            bNamed = false;
        if (!ResolveImageSource(InContext, m_Texture, Texture, Sheet, Frame, lImage, bNamed) && bNamed)
        {
            InvalidateContent();
            return;
        }

        // A border is a statement about ART, so it needs an image to measure against — the FRAME's
        // size for a sheet; without one this is the single quad U1 shipped (**UI20**).
        if (!Border.IsZero() && lImage.Texture != nullptr)
        {
            BuildSlicedQuads(GetBounds(), Border, lImage.SizePx, OutQuads);
        }
        else
        {
            OutQuads.emplace_back().Bounds = GetBounds();
        }

        for (UIQuad& lQuad : OutQuads)
        {
            lQuad.Color   = Color;
            lQuad.Texture = lImage.Texture;
        }

        // The slice's UVs are 0..1 of the image; a frame is a sub-rect of its sheet's texture.
        MapQuadUVsInto(OutQuads, lImage.UVMin, lImage.UVMax);

        // The fill is a CLIP over whatever was emitted, not a second geometry path: it crops from
        // the min edge so the bar empties toward it, and the UVs go with it so the texture is cut
        // rather than squashed — for nine quads exactly as for one.
        if (Fill != EUIFill::None && lAmount < 1.f)
        {
            ClipQuadsTo(OutQuads, FillClip(GetBounds(), Fill, lAmount));
        }
    }
}
