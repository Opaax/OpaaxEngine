#include "UI/Widgets/UIImage.h"

#include <algorithm>

#include "Core/Reflection/OpaaxEnumJson.h"
#include "RHI/Texture.h"
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

    void UIImage::SaveFields(nlohmann::json& InOutJson) const
    {
        UIWidget::SaveFields(InOutJson);

        // The PATH is written; the runtime pointer beside it is not, because it is a borrowed
        // handle that only code can hand over (**UI17**).
        InOutJson["Color"]      = Color;
        InOutJson["Texture"]    = Texture;
        InOutJson["Border"]     = Border;
        InOutJson["Fill"]       = Fill;
        InOutJson["FillAmount"] = FillAmount;
    }

    void UIImage::LoadFields(const nlohmann::json& InJson)
    {
        UIWidget::LoadFields(InJson);

        Color      = InJson.value("Color", Color);
        Texture    = InJson.value("Texture", Texture);
        Border     = InJson.value("Border", Border);
        Fill       = InJson.value("Fill", Fill);
        FillAmount = InJson.value("FillAmount", FillAmount);
    }

    void UIImage::Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        const float lAmount = Fill == EUIFill::None ? 1.f : std::clamp(FillAmount, 0.f, 1.f);
        if (lAmount <= 0.f)
        {
            return;
        }

        // The runtime pointer WINS; otherwise the authored path is resolved through the host.
        ITexture2D* lTexture = m_Texture;

        if (lTexture == nullptr && !Texture.IsEmpty() && InContext.Assets != nullptr)
        {
            lTexture = InContext.Assets->ResolveTexture(Texture.Path.CStr());

            if (lTexture == nullptr)
            {
                // Still uploading (or missing): draw nothing and ask again next frame (**UI3**).
                InvalidateContent();
                return;
            }
        }

        // A border is a statement about ART, so it needs a texture to measure against; without one
        // this is the single quad U1 shipped (**UI20**).
        if (!Border.IsZero() && lTexture != nullptr)
        {
            const Vector2F lTextureSize{ static_cast<float>(lTexture->GetWidth()),
                                         static_cast<float>(lTexture->GetHeight()) };

            BuildSlicedQuads(GetBounds(), Border, lTextureSize, OutQuads);
        }
        else
        {
            OutQuads.emplace_back().Bounds = GetBounds();
        }

        for (UIQuad& lQuad : OutQuads)
        {
            lQuad.Color   = Color;
            lQuad.Texture = lTexture;
        }

        // The fill is a CLIP over whatever was emitted, not a second geometry path: it crops from
        // the min edge so the bar empties toward it, and the UVs go with it so the texture is cut
        // rather than squashed — for nine quads exactly as for one.
        if (Fill != EUIFill::None && lAmount < 1.f)
        {
            ClipQuadsTo(OutQuads, FillClip(GetBounds(), Fill, lAmount));
        }
    }
}
